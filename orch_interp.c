/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "orch.h"

#include <lauxlib.h>
#include <lualib.h>

#ifndef ORCHLUA_PATH
#define	ORCHLUA_PATH ""
#endif

static const char orchlua_path[] = ORCHLUA_PATH;

static const char *
orch_interp_script(const char *orch_invoke_path)
{
	static char buf[MAXPATHLEN];

	/* Populate buf first... we cache the script path */
	if (buf[0] == '\0') {
		const char *path;

		path = getenv("ORCHLUA_PATH");
		if (path != NULL && (path[0] == '\0' || path[0] != '/')) {
			fprintf(stderr,
			    "Ignoring empty or relative ORCHLUA_PATH in the environment ('%s')\n",
			    path);
			path = NULL;
		}

		/* Fallback to what's built-in, if no env override. */
		if (path == NULL)
			path = orchlua_path;

		/* If ORCHLUA_PATH is empty, it's in the same path as our binary. */
		if (path[0] == '\0') {
			char *slash;

			if (realpath(orch_invoke_path, buf) == NULL)
				err(1, "realpath %s", orch_invoke_path);

			/* buf now a path to our binary, strip it. */
			slash = strrchr(buf, '/');
			if (slash == NULL)
				errx(1, "failed to resolve orch binary path");

			slash++;
			assert(*slash != '\0');
			*slash = '\0';
		} else {
			strlcpy(buf, path, sizeof(buf));
		}

		strlcat(buf, "/orch.lua", sizeof(buf));
	}

	return (&buf[0]);
}

int
orch_interp(const char *scriptf, const char *orch_invoke_path,
    int argc, const char * const argv[])
{
	lua_State *L;
	int status;

	L = luaL_newstate();
	if (L == NULL)
		errx(1, "luaL_newstate: out of memory");

	orchlua_configure(&(struct orch_interp_cfg) {
		.argc = argc,
		.argv = argv,
	});

	/* Open lua's standard library */
	luaL_openlibs(L);

	/* As well as our internal library */
	luaL_requiref(L, ORCHLUA_MODNAME, luaopen_orch, 0);
	lua_pop(L, 1);

	if (luaL_dofile(L, orch_interp_script(orch_invoke_path)) != LUA_OK) {
		const char *err;

		err = lua_tostring(L, -1);
		if (err == NULL)
			err = "unknown";

		status = 1;
		fprintf(stderr, "%s\n", err);
	} else {
		/*
		 * orch table is now at the top of stack, fetch run_script()
		 * and call it.
		 */
		lua_getfield(L, -1, "run_script");
		lua_pushstring(L, scriptf);
		lua_call(L, 1, 1);
		status = lua_toboolean(L, -1) ? 0 : 1;
	}

	lua_close(L);
	return (status);
}
