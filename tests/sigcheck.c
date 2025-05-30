/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Output various information about the current signal mask.
 *
 * -b: output only the numbers of signals that are currently blocked
 * -c: output only the count of signals that are currently blocked
 * normal operation: output the blocked/unblocked status of each signal
 */

static void
usage(int error)
{
	FILE *fp = stderr;

	if (error == 0)
		fp = stdout;
	fprintf(fp, "usage: %s [-b | -c]\n", getprogname());
	exit(error);
}

int
main(int argc, char *argv[])
{

	sigset_t sigmask;
	int ch, error, output = 0;
	enum {
		MODE_NORMAL = 0,
		MODE_BLOCKEDOUT,
		MODE_COUNT,
	} curmode = MODE_NORMAL;

	while ((ch = getopt(argc, argv, "bch")) != -1) {
		switch (ch) {
		case 'b':
			curmode = MODE_BLOCKEDOUT;
			break;
		case 'c':
			curmode = MODE_COUNT;
			break;
		case 'h':
			usage(0);
			break;
		default:
			usage(1);
		}
	}

	if (sigprocmask(SIG_SETMASK, NULL, &sigmask) != 0) {
		fprintf(stderr, "sigblock: %s\n", strerror(errno));
		return (1);
	}

	for (int signo = 1; signo < INT_MAX; signo++) {
		bool blocked;

		error = sigismember(&sigmask, signo);
		if (error == -1)
			break;

		blocked = !!error;
		switch (curmode) {
		case MODE_BLOCKEDOUT:
			if (!blocked)
				continue;
			printf("%s%d", output > 0 ? " " : "", signo);
			output++;
			break;
		case MODE_COUNT:
			if (blocked)
				output++;
			break;
		default:
			if (blocked)
				printf("Signal %d is blocked\n", signo);
			else
				printf("Signal %d is not blocked\n", signo);
			break;
		}
	}

	if (curmode == MODE_COUNT)
		printf("%d signal%s blocked\n", output, output != 1 ? "s" : "");

	return (0);
}
