/*-
 * Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>

#include <signal.h>

/* porch_signals.c */
const char * const *porch_signames(size_t *);
int porch_fetch_sigcaught(sigset_t *);
void porch_mask_apply(bool, sigset_t *, const sigset_t *);
int porch_sigmax(void);
