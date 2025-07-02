/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

extern enum porch_mode {
	PMODE_LOCAL,
	PMODE_REMOTE,
	PMODE_GENERATE,
} porch_mode;

extern const char *porch_rsh;

/* porch_interp.c */
void porch_interp_include(const char *);
int porch_interp(const char *, const char *, int, const char * const []);
