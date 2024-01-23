/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "orch.h"
#include "orch_lib.h"

#ifdef __OpenBSD__
#define	POSIX_OPENPT_FLAGS	(O_RDWR | O_NOCTTY)
#else
#define	POSIX_OPENPT_FLAGS	(O_RDWR | O_NOCTTY | O_CLOEXEC)
#endif

extern char **environ;

/* Parent */
static int orch_newpt(void);

/* Child */
static pid_t orch_newsess(void);
static void orch_usept(pid_t, int);
static void orch_exec(int, const char *[], int);

/* Both */
static void orch_release(int);
static void orch_wait(int);

int
orch_spawn(int argc, const char *argv[], struct orch_process *p)
{
	int cmdsock[2];
	pid_t pid, sess;

#ifdef SOCK_CLOEXEC
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, &cmdsock[0]) == -1)
		err(1, "socketpair");
#else
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, &cmdsock[0]) == -1)
		err(1, "socketpair");
	if (fcntl(cmdsock[0], F_SETFD, fcntl(cmdsock[0], F_GETFD) |
	    FD_CLOEXEC) == -1)
		err(1, "fcntl");
	if (fcntl(cmdsock[1], F_SETFD, fcntl(cmdsock[1], F_GETFD) |
	    FD_CLOEXEC) == -1)
		err(1, "fcntl");
#endif

	p->termctl = orch_newpt();

	pid = fork();
	if (pid == -1) {
		err(1, "fork");
	} else if (pid == 0) {
		/* Child */
		close(cmdsock[0]);
		sess = orch_newsess();

		orch_usept(sess, p->termctl);
		close(p->termctl);
		p->termctl = -1;

		orch_exec(argc, argv, cmdsock[1]);
	}

	p->released = false;
	p->pid = pid;
	p->cmdsock = cmdsock[0];

	/* Parent */
	close(cmdsock[1]);

	/*
	 * Stalls until the tty is configured, completely side step races from
	 * script writing to the tty before, e.g., echo is disabled.
	 */
	/* XXX Don't fail? */
	orch_wait(p->cmdsock);
	return (0);
}

static void
orch_wait(int cmdsock)
{
	ssize_t nb;
	int buf;

	nb = read(cmdsock, &buf, sizeof(buf));
	if (nb < 0)
		err(1, "read");
	else if ((size_t)nb < sizeof(buf) || buf != 0)
		errx(1, "protocol violation");
}

static void
orch_release(int cmdsock)
{
	ssize_t wsz;
	int buf = 0;

	/* XXX */
	wsz = write(cmdsock, &buf, sizeof(buf));
	(void)wsz;
}

static void
orch_exec(int argc __unused, const char *argv[], int cmdsock)
{

	/* Let the script commence. */
	orch_release(cmdsock);

	/*
	 * The child waits here for the script to release it.  It will typically be
	 * released on first match, but we provide an explicit release() function to
	 * do it manually in case the script doesn't want to queue up input before
	 * execution starts for some reason.
	 *
	 * For now this is just a simple int, in the future it may grow a more
	 * extensive protocol so that the script can, e.g., reconfigure the tty.
	 */
	orch_wait(cmdsock);

	execvp(argv[0], (char * const *)(const void *)argv);

	_exit(1);
}

static int
orch_newpt(void)
{
	int newpt;

	newpt = posix_openpt(POSIX_OPENPT_FLAGS);
	if (newpt == -1)
		err(1, "posix_openpt");
#if (POSIX_OPENPT_FLAGS & O_CLOEXEC) == 0
	if (fcntl(newpt, F_SETFD, fcntl(newpt, F_GETFD) | FD_CLOEXEC) == -1)
		err(1, "fcntl");
#endif

	if (grantpt(newpt) == -1)
		err(1, "grantpt");
	if (unlockpt(newpt) == -1)
		err(1, "unlockpt");

	return (newpt);
}

static pid_t
orch_newsess(void)
{
	pid_t sess;

	sess = setsid();
	if (sess == -1)
		err(1, "sess");

	return (sess);
}

static void
orch_usept(pid_t sess, int termctl)
{
	struct termios t;
	const char *name;
	int target;

	name = ptsname(termctl);
	if (name == NULL)
		err(1, "ptsname");

	target = open(name, O_RDWR);
	if (target == -1)
		err(1, "open: %s", name);

	if (tcsetsid(target, sess) == -1)
		err(1, "tcsetsid");

	if (tcgetattr(target, &t) == -1)
		err(1, "tcgetattr");

	t.c_lflag &= ~ECHO;

	if (tcsetattr(target, TCSANOW, &t) == -1)
		err(1, "tcsetattr");

	/* XXX Accept mask, buffering? */
	dup2(target, STDIN_FILENO);
	dup2(target, STDOUT_FILENO);
	dup2(target, STDERR_FILENO);
	if (target > STDERR_FILENO)
		close(target);
}
