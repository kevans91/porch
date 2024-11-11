/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Basically printenv(1), but we want a lexicographical sort so that our test
 * suite can cleanly match or not match some vars it expects to find based on
 * the environment.
 */

extern char **environ;

static int
envsort(const void *lenvp, const void *renvp)
{
	const char *left = *(const char **)lenvp;
	const char *right = *(const char **)renvp;

	return (strcmp(left, right));
}

int
main(int argc, char *argv[])
{
	size_t envc = 0;

	while (environ[envc] != NULL)
		envc++;

	qsort(environ, envc, sizeof(*environ), envsort);

	for (size_t i = 0; i < envc; i++)
		printf("%s\n", environ[i]);

	return (0);
}
