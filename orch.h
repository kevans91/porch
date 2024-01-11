/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <lua.h>

/* orch_interp.c */
int orch_interp(const char *, int);

/* orch_lua.c */
void luaopen_orch(lua_State *, int);
size_t orch_minbufsize(lua_State *);
