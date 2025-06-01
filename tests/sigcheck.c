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

#define	SA_SIG_IGN	((void (*)(int, siginfo_t *, void *))SIG_IGN)

/*
 * Output various information about the current signal mask.
 *
 * -b: output only the numbers of signals that are currently blocked
 * -c: output only the count of signals that are currently blocked
 * -i: output only the numbers of signals that are currently ignored
 * normal operation: output the blocked/unblocked status of each signal
 */

static void
usage(const char *progname, int error)
{
	FILE *fp = stderr;

	if (error == 0)
		fp = stdout;
	fprintf(fp, "usage: %s [-b | -c]\n", progname);
	exit(error);
}

int
main(int argc, char *argv[])
{
	sigset_t sigmask;
	struct sigaction act;
	int ch, error, nblocked = 0, nignored = 0, output = 0;
	enum {
		MODE_NORMAL = 0,
		MODE_BLOCKEDOUT,
		MODE_IGNOREDOUT,
		MODE_COUNT,
	} curmode = MODE_NORMAL;

	while ((ch = getopt(argc, argv, "bchi")) != -1) {
		switch (ch) {
		case 'b':
			curmode = MODE_BLOCKEDOUT;
			break;
		case 'c':
			curmode = MODE_COUNT;
			break;
		case 'i':
			curmode = MODE_IGNOREDOUT;
			break;
		case 'h':
			usage(argv[0], 0);
			break;
		default:
			usage(argv[0], 1);
		}
	}

	if (sigprocmask(SIG_SETMASK, NULL, &sigmask) != 0) {
		fprintf(stderr, "error: sigblock: %s\n", strerror(errno));
		return (1);
	}

	for (int signo = 1; signo < INT_MAX; signo++) {
		bool blocked, ignored;

		error = sigismember(&sigmask, signo);
		if (error == -1)
			break;

		blocked = !!error;

		/*
		 * Not all signals can be examined, just pretend it's ignored
		 * if we know the signal is valid.
		 */
		if (sigaction(signo, NULL, &act) == -1)
			ignored = false;
		else if ((act.sa_flags & SA_SIGINFO) != 0)
			ignored = act.sa_sigaction == SA_SIG_IGN;
		else
			ignored = act.sa_handler == SIG_IGN;

		if (blocked)
			nblocked++;
		if (ignored)
			nignored++;

		switch (curmode) {
		case MODE_BLOCKEDOUT:
			if (!blocked)
				continue;
			printf("%s%d", nblocked > 1 ? " " : "", signo);
			break;
		case MODE_IGNOREDOUT:
			if (!ignored)
				continue;
			printf("%s%d", nignored > 1 ? " " : "", signo);
			break;
		case MODE_COUNT:
			break;
		default:
			if (blocked)
				printf("Signal %d is blocked\n", signo);
			else
				printf("Signal %d is not blocked\n", signo);
			if (ignored)
				printf("Signal %d is ignored\n", signo);
			else
				printf("Signal %d is not ignored\n", signo);
			break;
		}
	}

	switch (curmode) {
	case MODE_BLOCKEDOUT:
		if (nblocked == 0)
			break;
		printf("\n");
		break;
	case MODE_IGNOREDOUT:
		if (nignored == 0)
			break;
		printf("\n");
		break;
	case MODE_COUNT:
		printf("%d signal%s blocked\n", nblocked,
		    nblocked != 1 ? "s" : "");
		printf("%d signal%s ignored\n", nignored,
		    nignored != 1 ? "s" : "");
		break;
	default:
		break;
	}
	return (0);
}
