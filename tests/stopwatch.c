/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Halts when the program has been stopped for at least 3 seconds, or up to a
 * configurable amount of time passed in as # seconds via the first argument.
 */
#define	SEC_TO_NSEC(x)	(1000000000ULL * (x))

/*
 * Delay certain amount in milliseconds, because I like milliseconds as a
 * compromise between nanoseconds and seconds.
 */
static void
delay(int millis)
{
	struct timespec ts;

	ts.tv_sec = millis / 1000;
	ts.tv_nsec = (millis % 1000) * 1000000;

	while (nanosleep(&ts, &ts) != 0) {
		if (errno == EINTR)
			continue;

		fprintf(stderr, "nanosleep: %s\n", strerror(errno));
		exit(1);
	}
}

static void
fill_timespec(struct timespec *tsp)
{
	int error;

	error = clock_gettime(CLOCK_MONOTONIC, tsp);
	if (error == -1) {
		fprintf(stderr, "clock_gettime: %s\n", strerror(errno));
		exit(1);
	}
}

static double
delta(const struct timespec *late, const struct timespec *early)
{
	struct timespec dts;

	assert(late->tv_sec >= early->tv_sec);
	dts.tv_sec = late->tv_sec - early->tv_sec;
	dts.tv_nsec = late->tv_nsec - early->tv_nsec;
	if (dts.tv_nsec < 0) {
		dts.tv_sec--;
		dts.tv_nsec += SEC_TO_NSEC(1);
	}

	return (dts.tv_sec + (((double)dts.tv_nsec) / SEC_TO_NSEC(1)));
}

int
main(int argc, char *argv[])
{
	struct timespec last, now;
	double last_diff;
	long time = 3;

	if (argc > 1) {
		char *endp;

		errno = 0;
		time = strtol(argv[1], &endp, 10);
		if (errno != 0 || *endp != '\0') {
			fprintf(stderr, "usage: %s [seconds]\n", argv[0]);
			return (1);
		}
	}

	printf("Timer starting\n");

	/*
	 * This is a simple implementation to try and measure gaps in execution
	 * driven by SIGSTOP/SIGCONT.  We effectively measure the time a single
	 * iteration has taken, and if it's taken at least as long as the
	 * caller requested then we bail out.  If we're hit with a SIGSTOP after
	 * `now` has been refilled at the end, it may simply take us another
	 * 250ms to notice that we had sufficient delay the time before.
	 */
	fill_timespec(&now);
	last = now;
	for (;;) {
		last_diff = delta(&now, &last);
		if (last_diff >= time)
			break;
		if (last_diff >= 0.50)
			printf("Delta %f (want %ld)\n", last_diff, time);
		delay(250);
		last = now;
		fill_timespec(&now);
	}

	printf("Timer finished, last delta: %f\n", last_diff);
	return (0);
}
