/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifdef __APPLE__
#include <ctype.h>
#include <err.h>
#endif
#include <signal.h>
#include <stdbool.h>
#include <string.h>

static const char *porch_platform_signames[NSIG];

#if !STATIC_SIGLIST
static bool signames_loaded;

static void
load_signames(void)
{

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__APPLE__)
	for (size_t i = 0; i < NSIG; i++) {
#ifdef __APPLE__
		char *signame;

		/*
		 * macOS is already known to lowercase their signames, so we must shim
		 * them out to uppercase.
		 */
		signame = strdup(sys_signame[i]);
		if (signame == NULL)
			err(1, "strdup");

		for (char *cp = signame; *cp != '\0'; cp++) {
			*cp = toupper(*cp);
		}

#else	/* !__APPLE__ */
		const char *signame = sys_signame[i];
#endif	/* __APPLE__ */

		porch_platform_signames[i] = signame;
	}
#elif defined(__linux__)
	for (size_t i = 0; i < NSIG; i++) {
		porch_platform_signames[i] = sigabbrev_np(i);
	}
#else
#error Platform signal name mapping mechanism not currently defined
#endif

	signames_loaded = true;
}
#endif	/* !STATIC_SIGLIST */

const char * const *
porch_signames(size_t *sigcnt)
{

#if !STATIC_SIGLIST
	if (!signames_loaded)
		load_signames();
#endif

	*sigcnt = NSIG;
	return (porch_platform_signames);
}
