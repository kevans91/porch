/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#ifdef __APPLE__
#include <ctype.h>
#include <err.h>
#endif
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>

#define	SA_SIG_IGN	((void (*)(int, siginfo_t *, void *))SIG_IGN)
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

int
porch_sigset2mask(const sigset_t *sigset)
{
	int error, mask = 0;

	/*
	 * For signal masks, we may support signals higher than NSIG if the
	 * platform allows it, so we'll aim towards INT_MAX and bail out as soon
	 * as sigismember() pushes back.
	 */
	for (int signo = 1; signo < INT_MAX; signo++) {
		error = sigismember(sigset, signo);
		if (error == -1)
			break;	/* Can't express any further in a mask. */
		if (error == 0)
			continue;	/* Not a member of the set. */

		/* Member of the set. */
		mask |= (1 << (signo - 1));
	}

	return (mask);
}

/*
 * Returns 0 if the mask was successfully converted, or ENOENT if a bit was
 * set in the mask that we can't represent.
 */
int
porch_mask2sigset(int mask, sigset_t *sigset)
{
	int signo;

	while (mask != 0) {
		signo = ffs(mask);
		if (sigaddset(sigset, signo) != 0)
			return (errno);

		mask &= ~(1 << (signo - 1));
	}

	return (0);
}

static bool
porch_sig_uncatchable(int signo)
{

	switch (signo) {
	case SIGKILL:
	case SIGSTOP:
		return (true);
	default:
		return (false);
	}
}

int
porch_fetch_sigcaught(sigset_t *sigset)
{
	struct sigaction act;

	sigemptyset(sigset);
	for (int signo = 1; signo < INT_MAX; signo++) {
		if (porch_sig_uncatchable(signo))
			continue;

		if (sigismember(sigset, signo) == -1)
			break;	/* Hit the end of valid signals. */
		if (sigaction(signo, NULL, &act) != 0) {
			/*
			 * For signals that we can't examine, we just pretend
			 * that they're caught for convenience.  Any attempt to
			 * change that might fail, the user kind of needs to
			 * know the platform they're running on.  porch itself
			 * won't try to do anything with them.
			 */
		} else if ((act.sa_flags & SA_SIGINFO) != 0) {
			if (act.sa_sigaction == SA_SIG_IGN)
				continue;
		} else if (act.sa_handler == SIG_IGN) {
			continue;
		}

		sigaddset(sigset, signo);
	}

	return (0);
}

/*
 * Apply the given mask to our sigset.  If complement is set, then we're
 * unsetting those bits that are set in the mask.
 */
void
porch_mask_apply(bool complement, sigset_t *sigset, int mask)
{
	int error, signo;

	while (mask != 0) {
		signo = ffs(mask);
		if (complement)
			error = sigdelset(sigset, signo);
		else
			error = sigaddset(sigset, signo);

		assert(error == 0);
		mask &= ~(1 << (signo - 1));
	}
}
