/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <err.h>

#include "orch.h"

#include <lauxlib.h>
#include <lualib.h>

#ifndef LIBEXEC_PATH
#define	LIBEXEC_PATH ""
#endif

int
orch_interp(const char *scriptf, int argc, const char * const argv[])
{
	lua_State *L;

	L = luaL_newstate();
	if (L == NULL)
		errx(1, "luaL_newstate: out of memory");

	orchlua_configure(&(struct orch_interp_cfg) {
	    .scriptf = scriptf,
		.argc = argc,
		.argv = argv,
	});

	/* Open lua's standard library */
	luaL_openlibs(L);

	/* As well as our internal library */
	luaL_requiref(L, ORCHLUA_MODNAME, luaopen_orch, 0);
	lua_pop(L, 1);

	if (luaL_dofile(L, LIBEXEC_PATH "orch.lua") != LUA_OK) {
		const char *err;

		err = lua_tostring(L, -1);
		if (err == NULL)
			err = "unknown";

		errx(1, "%s", err);
	}

	/* Should not be reachable... we go through impl.exit. */
	return (1);
}
