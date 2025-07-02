/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "porch.h"
#include "porch_bin.h"

#ifndef __dead2
#define	__dead2	__attribute__((noreturn))
#endif

static const char *porch_shortopts = "f:i:hV";
static const char *porchgen_shortopts = "f:hV";
static const char *rporch_shortopts = "e:f:i:hV";

enum porch_mode porch_mode = PMODE_LOCAL;
const char *porch_rsh;

static void __dead2
usage(const char *name, int error)
{
	FILE *f;

	if (error == 0)
		f = stdout;
	else
		f = stderr;

	switch (porch_mode) {
	case PMODE_REMOTE:
		fprintf(f, "usage: %s [-e rsh] [-f file] [-i include] [host]\n", name);
		break;
	case PMODE_GENERATE:
		fprintf(f, "usage: %s -f file command [argument ...]\n",
		    name);
		break;
	case PMODE_LOCAL:
		fprintf(f, "usage: %s [-f file] [-i include] [command [argument ...]]\n",
		    name);
		break;
	}

	exit(error);
}

static void __dead2
version(void)
{
	printf("porch v%s\n", PORCH_VERSION);
	exit(0);
}

int
main(int argc, char *argv[])
{
	const char *invoke_base, *invoke_path = argv[0];
	const char *scriptf;
	const char *shortopts;
	int ch;

	if (argc == 0)
		usage("<empty>", 1);

	invoke_base = strrchr(invoke_path, '/');
	if (invoke_base != NULL)
		invoke_base++;
	else
		invoke_base = invoke_path;

	if (strcmp(invoke_base, "rporch") == 0)
		porch_mode = PMODE_REMOTE;
	else if (strcmp(invoke_base, "porchgen") == 0)
		porch_mode = PMODE_GENERATE;

	switch (porch_mode) {
	case PMODE_REMOTE:
		shortopts = rporch_shortopts;
		break;
	case PMODE_GENERATE:
		shortopts = porchgen_shortopts;
		scriptf = NULL;	/* Must be specified. */
		break;
	default:
		shortopts = porch_shortopts;
		scriptf = "-";	/* stdin */
		break;
	}

	while ((ch = getopt(argc, argv, shortopts)) != -1) {
		switch (ch) {
		case 'e':
			porch_rsh = optarg;
			break;
		case 'f':
			scriptf = optarg;
			break;
		case 'i':
			porch_interp_include(optarg);
			break;
		case 'h':
			usage(invoke_path, 0);
		case 'V':
			version();
		default:
			usage(invoke_path, 1);
		}
	}

	argc -= optind;
	argv += optind;

	switch (porch_mode) {
	case PMODE_REMOTE:
		/*
		 * May have a host specified to execute the script on.  We
		 * explicitly allow no host in case the rsh script is designed
		 * to connect to a single remote host without a host argument.
		 * We don't allow more than one host.
		 */
		if (argc > 1)
			usage(invoke_path, 1);

		/*
		 * We prefer an rsh specified via -e, but if omitted then we'll
		 * take PORCH_RSH or default to "ssh".
		 */
		if (porch_rsh == NULL || porch_rsh[0] == '\0')
			porch_rsh = getenv("PORCH_RSH");
		if (porch_rsh == NULL || porch_rsh[0] == '\0')
			porch_rsh = "ssh";
		break;
	case PMODE_GENERATE:
		if (argc == 0 || scriptf == NULL)
			usage(invoke_path, 1);
		break;
	default:
		break;
	}

	/*
	 * If we have a command supplied, we'll spawn() it for the script just to
	 * simplify things.  If we didn't, then the script just needs to make sure
	 * that it spawns something before a match/one block.
	 */
	return (porch_interp(scriptf, invoke_path, argc,
	    (const char * const *)argv));
}
