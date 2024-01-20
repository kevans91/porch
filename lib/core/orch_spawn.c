/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "orch.h"
#include "orch_lib.h"

#ifdef __OpenBSD__
#define	POSIX_OPENPT_FLAGS	(O_RDWR | O_NOCTTY)
#else
#define	POSIX_OPENPT_FLAGS	(O_RDWR | O_NOCTTY | O_CLOEXEC)
#endif

/* A bit lazy, but meh. */
#if defined(SOCK_CLOEXEC) && defined(SOCK_NONBLOCK)
#define	SOCKPAIR_ATTRS	(SOCK_CLOEXEC | SOCK_NONBLOCK)
#elif defined(SOCK_CLOEXEC)
#define	SOCKPAIR_ATTRS	(SOCK_CLOEXEC)
#elif defined(SOCK_NONBLOCK)
#define	SOCKPAIR_ATTRS	(SOCK_NONBLOCK)
#else
#define	SOCKPAIR_ATTRS	(0)
#endif

extern char **environ;

/* Parent */
static int orch_newpt(void);

/* Child */
static pid_t orch_newsess(void);
static void orch_usept(pid_t, int, struct termios *);
static void orch_child_error(const char *, ...) __printflike(1, 2);
static void orch_exec(int, const char *[], struct termios *t);

/* Both */
static int orch_wait(void);

int
orch_spawn(int argc, const char *argv[], struct orch_process *p)
{
	int cmdsock[2];
	pid_t pid, sess;

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCKPAIR_ATTRS, 0,
	    &cmdsock[0]) == -1)
		err(1, "socketpair");
#if (SOCKPAIR_ATTRS & SOCK_CLOEXEC) == 0
	if (fcntl(cmdsock[0], F_SETFD, fcntl(cmdsock[0], F_GETFD) |
	    FD_CLOEXEC) == -1)
		err(1, "fcntl");
	if (fcntl(cmdsock[1], F_SETFD, fcntl(cmdsock[1], F_GETFD) |
	    FD_CLOEXEC) == -1)
		err(1, "fcntl");
#endif
#if (SOCKPAIR_ATTRS & SOCK_NONBLOCK) == 0
	if (fcntl(cmdsock[0], F_SETFL, fcntl(cmdsock[0], F_GETFL) |
	    O_NONBLOCK) == -1)
		err(1, "fcntl");
	if (fcntl(cmdsock[1], F_SETFL, fcntl(cmdsock[1], F_GETFL) |
	    O_NONBLOCK) == -1)
		err(1, "fcntl");
#endif

	p->termctl = orch_newpt();

	pid = fork();
	if (pid == -1) {
		err(1, "fork");
	} else if (pid == 0) {
		struct termios t;

		/* Child */
		close(cmdsock[0]);
		orch_ipc_open(cmdsock[1]);

		sess = orch_newsess();

		orch_usept(sess, p->termctl, &t);
		close(p->termctl);
		p->termctl = -1;

		orch_exec(argc, argv, &t);
	}

	p->released = false;
	p->pid = pid;
	orch_ipc_open(cmdsock[0]);

	/* Parent */
	close(cmdsock[1]);

	/*
	 * Stalls until the tty is configured, completely side step races from
	 * script writing to the tty before, e.g., echo is disabled.
	 */
	return (orch_wait());
}

static int
orch_wait(void)
{
	struct orch_ipc_msg *msg;
	bool stop = false;

	while (!stop) {
		if (orch_ipc_wait(&stop) == -1)
			return (-1);
		else if (stop)
			break;

		if (orch_ipc_recv(&msg) != 0)
			return (-1);
		if (msg == NULL)
			continue;

		stop = msg->hdr.tag == IPC_RELEASE;

		free(msg);
		msg = NULL;
	}

	return (0);
}

int
orch_release(void)
{
	struct orch_ipc_msg msg;

	msg.hdr.size = sizeof(msg.hdr);
	msg.hdr.tag = IPC_RELEASE;

	return (orch_ipc_send(&msg));
}

static void
orch_child_error(const char *fmt, ...)
{
	struct orch_ipc_msg *errmsg;
	char *str, *msgstr;
	va_list ap;
	int sz;

	errmsg = NULL;
	va_start(ap, fmt);
	if ((sz = vasprintf(&str, fmt, ap)) == -1)
		goto out;
	va_end(ap);

	errmsg = malloc(sizeof(errmsg->hdr) + sz + 1);
	if (errmsg == NULL)
		goto out;

	errmsg->hdr.tag = IPC_ERROR;
	errmsg->hdr.size = sizeof(errmsg->hdr) + sz + 1;
	msgstr = (void *)(errmsg + 1);
	strlcpy(msgstr, str, sz + 1);

	free(str);
	str = NULL;

	orch_ipc_send(errmsg);

out:
	free(errmsg);
	free(str);
	orch_ipc_close();
	_exit(1);
}

static int
orch_child_termios_inquiry(struct orch_ipc_msg *inmsg __unused, void *cookie)
{
	struct orch_ipc_msg *msg;
	struct termios *child_termios = cookie, *parent_termios;
	size_t msgsz;
	int error, serr;

	/* Send term attributes back over the wire. */
	msgsz = sizeof(msg->hdr) + sizeof(*child_termios);
	msg = malloc(msgsz);
	if (msg == NULL) {
		errno = EINVAL;
		return (-1);
	}

	msg->hdr.tag = IPC_TERMIOS_SET;
	msg->hdr.size = msgsz;
	parent_termios = (void *)(msg + 1);
	memcpy(parent_termios, child_termios, sizeof(*child_termios));

	error = orch_ipc_send(msg);
	serr = errno;

	free(msg);
	if (error != 0)
		errno = serr;
	return (error);
}

static int
orch_child_termios_set(struct orch_ipc_msg *msg, void *cookie)
{
	struct termios *updated_termios;
	struct termios *current_termios = cookie;
	struct orch_ipc_msg ack = {
		.hdr = { .size = sizeof(ack), .tag = IPC_TERMIOS_ACK }
	};
	size_t datasz = msg->hdr.size - sizeof(msg->hdr);

	if (datasz != sizeof(*updated_termios)) {
		errno = EINVAL;
		return (-1);
	}

	updated_termios = (void *)(msg + 1);

	/*
	 * We don't need to keep track of the updated state, but we do so
	 * anyways.
	 */
	memcpy(current_termios, updated_termios, sizeof(*updated_termios));

	if (tcsetattr(STDIN_FILENO, TCSANOW, current_termios) == -1)
		orch_child_error("tcsetattr");

	return (orch_ipc_send(&ack));
}

static void
orch_exec(int argc __unused, const char *argv[], struct termios *t)
{
	int error;

	/*
	 * Register a couple of events that the script may want to use:
	 * - IPC_TERMIOS_INQUIRY: sent our terminal attributes back over.
	 * - IPC_TERMIOS_SET: update our terminal attributes
	 */
	orch_ipc_register(IPC_TERMIOS_INQUIRY, orch_child_termios_inquiry, t);
	orch_ipc_register(IPC_TERMIOS_SET, orch_child_termios_set, t);

	/* Let the script commence. */
	if (orch_release() != 0)
		_exit(1);

	/*
	 * The child waits here for the script to release it.  It will typically be
	 * released on first match, but we provide an explicit release() function to
	 * do it manually in case the script doesn't want to queue up input before
	 * execution starts for some reason.
	 *
	 * For now this is just a simple int, in the future it may grow a more
	 * extensive protocol so that the script can, e.g., reconfigure the tty.
	 */
	error = orch_wait();
	orch_ipc_close();

	if (error != 0)
		_exit(1);

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
		orch_child_error("setsid");

	return (sess);
}

static void
orch_usept(pid_t sess, int termctl, struct termios *t)
{
	const char *name;
	int target;

	name = ptsname(termctl);
	if (name == NULL)
		orch_child_error("ptsname: %s", strerror(errno));

	target = open(name, O_RDWR);
	if (target == -1)
		orch_child_error("open %s: %s", name, strerror(errno));

	if (tcsetsid(target, sess) == -1)
		orch_child_error("tcsetsid");

	if (tcgetattr(target, t) == -1)
		orch_child_error("tcgetattr");

	/* XXX Accept mask, buffering? */
	dup2(target, STDIN_FILENO);
	dup2(target, STDOUT_FILENO);
	dup2(target, STDERR_FILENO);
	if (target > STDERR_FILENO)
		close(target);
}
