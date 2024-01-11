/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/queue.h>

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "orch.h"

#include <lauxlib.h>

#define	ORCHLUA_MATCHKEY	"matches"
#define	ORCHLUA_MINBUFSZKEY	"minbufsz"

#define	ORCHLUA_MODNAME		"orch"

static int orch_termctl = -1;
#if 0
	char *buf;
	size_t wantsz;
	ssize_t readsz;

	buf = malloc(wantsz);
	if (buf == NULL) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return (2);
	}

	off = 0;
	while (wantsz != 0) {
		readsz = read(arch_termctl, &buf[off], wantsz);
		if (readsz == 0) {
			free(buf);
			lua_pushnil(L);
			lua_pushstring(L, "end of file");
			return (2);
		}

		wantsz -= readsz;
		off += readsz;
	}

	matches = memcmp(buf,
#endif

static int
orchua_addmatch(lua_State *L)
{

}

static int
orchlua_newmatch(lua_State *L)
{
	unsigned int idx;

	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getglobal(L, ORCHLUA_MODNAME);
	lua_getfield(L, -1, ORCHLUA_MATCHKEY);
	idx = lua_rawlen(L, -1) + 1;

	/* Grow a slot for the table */
	lua_settop(L, lua_gettop(L) + 1);
	lua_copy(L, 1, -1);
	lua_seti(L, -2, idx);

	/* Drop the matches table + orch global */
	lua_pop(L, 2);
	lua_pushcclosure(L, orchlua_newmatch, 0);
	return (1);
}

static int
orchlua_match(lua_State *L)
{

	/*
	 * Clear the matches table.  This is safe even if we're nested in another
	 * match action, because we won't try to access the matches table after.
	 */
	lua_getglobal(L, ORCHLUA_MODNAME);
	lua_newtable(L);
	lua_setfield(L, -2, ORCHLUA_MATCHKEY);

	/* Drop the table for now; newmatch will pick it up again. */
	lua_pop(L, 1);

	return (orchlua_newmatch(L));
}

#if 0
static int
orchlua_write(lua_State *L)
{
}
#endif

#define	REG_SIMPLE(n)	{ #n, orchlua_ ## n }
static const struct luaL_Reg orchlib[] = {
	REG_SIMPLE(match),
#if 0
	REG_SIMPLE(write),
#endif
	{ NULL, NULL },
};

#if 0
static void
orchlua_exechook(lua_State *L, lua_Debug *ar)
{

	lua_getinfo(L, "n", ar);
	fprintf(stderr, "name '%s'\n", ar->name);
}
#endif

void
luaopen_orch(lua_State *L, int termctl)
{

	assert(termctl >= 0);

	luaL_newlib(L, orchlib);
	lua_newtable(L);
	lua_setfield(L, -2, ORCHLUA_MATCHKEY);
	lua_setglobal(L, ORCHLUA_MODNAME);

	orch_termctl = termctl;
#if 0
	lua_sethook(L, orchlua_exechook, LUA_MASKRET, 0);
#endif
}
