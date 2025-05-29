/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* sys/cdefs.h */
#ifndef __unused
#define	__unused	__attribute__((unused))
#endif

static sig_atomic_t signals;

static void
catch_sigint(int signo __unused)
{

	/* Not async-signal-safe */
	printf("\nInterrupt caught\n");
	signals++;
}

static void
catch_sigusr(int signo)
{

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	if (signo == SIGUSR1)
		sleep(3);

	signal(signo, SIG_DFL);
	raise(signo);
}

int
main()
{
	struct sigaction sa = {
	    .sa_handler = catch_sigint,
	};
	char *line;
	ssize_t readsz;
	size_t linesz;

	sigaction(SIGINT, &sa, NULL);

	signal(SIGUSR1, catch_sigusr);
	signal(SIGUSR2, catch_sigusr);

	linesz = 0;
	line = NULL;
	while (signals < 3) {
		printf(">> ");

		readsz = getline(&line, &linesz, stdin);
		if (readsz == 0)
			continue;
		if (readsz < 0) {
			if (errno != EINTR) {
				free(line);
				fprintf(stderr, "getline: %s\n", strerror(errno));
				return (1);
			}

			/*
			 * glibc at least will persist an EINTR so that we end
			 * up endlessly looping if we don't clear it.
			 */
			clearerr(stdin);
			continue;
		}

		line[readsz - 1] = '\0';
		printf("%s\n", line);
	}

	free(line);
	return (37);
}
