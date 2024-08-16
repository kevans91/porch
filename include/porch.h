/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>

#include <stdbool.h>

#include <lua.h>

#define	PORCHLUA_MODNAME	"porch.core"
#define	PORCHTTY_MODNAME	"porch.tty"

/* sys/cdefs.h */
#ifndef __unused
#define	__unused	__attribute__((unused))
#endif
#ifndef __printflike
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif

/* porch_compat.c */
#ifdef __linux__
size_t strlcpy(char * __restrict, const char * __restrict, size_t);
size_t strlcat(char * __restrict, const char * __restrict, size_t);
#endif
#if defined(__linux__) || defined(__APPLE__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
int tcsetsid(int, int);
#endif

/* porch_lua.c */
int luaopen_porch_core(lua_State *);
int luaopen_porch_tty(lua_State *);
