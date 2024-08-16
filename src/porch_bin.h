/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <lua.h>

/* porch_interp.c */
int porch_interp(const char *, const char *, int, const char * const []);
