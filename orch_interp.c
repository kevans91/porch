/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <err.h>

#include "orch.h"

#include <lauxlib.h>

static int orch_loop(lua_State *, int);

int
orch_interp(const char *scriptf, int termctl)
{
	lua_State *L;

	L = luaL_newstate();
	if (L == NULL)
		errx(1, "luaL_newstate: out of memory");

	luaL_openlibs(L);
	luaopen_orch(L, termctl);

	if (luaL_dofile(L, scriptf) != LUA_OK) {
		const char *err;

		err = lua_tostring(L, -1);
		if (err == NULL)
			err = "unknown";

		errx(1, "%s", err);
	}

	return (orch_loop(L, termctl));
}

static int
orch_loop(lua_State *L, int termctl)
{

	return (0);
}
