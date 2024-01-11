/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "orch.h"

static void orch_exec(int argc, char *argv[]);
static int orch_newpt(void);
static pid_t orch_newsess(void);
static void orch_usept(pid_t sess, int termctl);

static void
usage(int error)
{
	FILE *f;

	if (error == 0)
		f = stdout;
	else
		f = stderr;

	fprintf(f, "usage: %s [-h] command [argument ...]", getprogname());
	exit(error);
}

int
main(int argc, char *argv[])
{
	int ch, termctl;
	pid_t pid, sess;

	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
			usage(0);
		default:
			usage(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage(1);

	termctl = orch_newpt();

	pid = fork();
	if (pid == -1) {
		err(1, "fork");
	} else if (pid == 0) {
		/* Child */
		sess = orch_newsess();

		orch_usept(sess, termctl);
		close(termctl);
		termctl = -1;

		orch_exec(argc, argv);
	}

	/* Parent */
	return (orch_interp("/dev/stdin", termctl));
}

static void
orch_exec(int argc, char *argv[])
{

	execvp(argv[0], argv);
	_exit(1);
}

static int
orch_newpt(void)
{
	int newpt;

	newpt = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (newpt == -1)
		err(1, "posix_openpt");

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
	const char *name;
	int target;

	name = ptsname(termctl);
	if (name == NULL)
		err(1, "ptsname");

	target = open(name, O_RDWR);
	if (target == -1)
		err(1, "%s", name);

	if (tcsetsid(target, sess) == -1)
		err(1, "tcsetsid");

	/* XXX Accept mask, buffering? */
	dup2(target, STDIN_FILENO);
	dup2(target, STDOUT_FILENO);
	dup2(target, STDERR_FILENO);
	if (target > STDERR_FILENO)
		close(target);
}
