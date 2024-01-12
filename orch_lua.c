/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/select.h>
#include <sys/queue.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "orch.h"

#include <lauxlib.h>

#define	ORCHLUA_MATCHKEY	"matches"
#define	ORCHLUA_MINBUFSZKEY	"minbufsz"

struct orch_interp_cfg orchlua_cfg;

static int orch_termctl = -1;

static int
orchlua_release(lua_State *L)
{
	ssize_t wsz;
	int buf = 0, err;

	if ((wsz = write(orchlua_cfg.cmdsock, &buf, sizeof(buf))) != sizeof(buf)) {
		err = errno;
		luaL_pushfail(L);
		if (wsz < 0)
			lua_pushstring(L, strerror(err));
		else
			lua_pushstring(L, "cmd socket closed");
		return (2);
	}

	lua_pushboolean(L, 1);
	return (1);
}

/*
 * Not exported
 */
static int
orchlua_close(lua_State *L)
{
	luaL_Stream *p;
	int ret;

	p = (luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE);
	ret = fclose(p->f);
	return (luaL_fileresult(L, ret == 0, NULL));
}

static int
orchlua_open(lua_State *L)
{
	luaL_Stream *p;
	const char *filename;
	int fd;

	filename = luaL_checkstring(L, 1);

	fd = -1;
	if (orchlua_cfg.dirfd == -1) {
		if (strcmp(filename, "-") != 0) {
			luaL_pushfail(L);
			lua_pushstring(L,
			    "No sandbox granted (script opened from stdin)");
			return (2);
		}

		fd = dup(STDIN_FILENO);
		if (fd == -1)
			return (luaL_fileresult(L, 0, "stdin"));
	}

	p = (luaL_Stream *)lua_newuserdata(L, sizeof(*p));
	p->closef = &orchlua_close;
	luaL_setmetatable(L, LUA_FILEHANDLE);

	if (fd == -1)
		fd = openat(orchlua_cfg.dirfd, filename, O_RDONLY);
	if (fd == -1)
		return (luaL_fileresult(L, 0, filename));

	p->f = fdopen(fd, "r");
	if (p->f == NULL)
		return (luaL_fileresult(L, 0, filename));

	return (1);
}

/*
 * read(callback[, timeout]) -- returns true if we finished, false if we
 * hit EOF, or a fail, error pair otherwise.
 */
static int
orchlua_read(lua_State *L)
{
	char buf[LINE_MAX];
	fd_set rfd;
	struct timeval tv, *tvp;
	time_t start, now;
	ssize_t readsz;
	int fd, ret;
	lua_Number timeout;

	luaL_checktype(L, 1, LUA_TFUNCTION);

	if (lua_gettop(L) > 1) {
		timeout = luaL_checknumber(L, 2);
		if (timeout < 0) {
			luaL_pushfail(L);
			lua_pushstring(L, "Invalid timeout");
			return (2);
		} else {
			timeout = MAX(timeout, 1);
		}

		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		tvp = &tv;
	} else {
		/* No timeout == block */
		tvp = NULL;
	}

	fd = orchlua_cfg.termctl;

	FD_ZERO(&rfd);

	/* Crude */
	if (tvp != NULL)
			start = now = time(NULL);
	while (tvp == NULL || now - start < timeout) {
		FD_SET(fd, &rfd);
		ret = select(fd + 1, &rfd, NULL, NULL, tvp);
		if (ret == -1 && errno == EINTR) {
			/*
			 * Rearm the timeout and go again; we'll just let the loop terminate
			 * with a negative tv.tv_sec if timeout seconds have elapsed.
			 */
			if (tvp != NULL) {
				now = time(NULL);
				tv.tv_sec = timeout - (now - start);
			}

			continue;
		} else if (ret == -1) {
			int err = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(err));
			return (2);
		} else if (ret == 0) {
			luaL_pushfail(L);
			lua_pushstring(L, "Timeout");
			return (2);
		}

		/* Read it */
		readsz = read(fd, buf, sizeof(buf));
		if (readsz < 0) {
			int err = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(err));
			return (2);
		} else {
			int nargs = 0;

			/*
			 * Duplicate the function value, it'll get popped by the call.
			 */
			lua_settop(L, 2);
			lua_copy(L, 1, 2);

			/* callback([data]) -- nil data == EOF */
			if (readsz > 0) {
				lua_pushlstring(L, buf, readsz);
				nargs++;
			}

			/*
			 * Callback should return true if it's done, false if it wants more.
			 */
			lua_call(L, nargs, 1);

			if (readsz == 0) {
				/* Don't care about the return value if we hit EOF. */
				lua_pushboolean(L, 0);

				close(orchlua_cfg.termctl);
				orchlua_cfg.termctl = -1;
				return (1);
			}

			if (lua_toboolean(L, -1))
				break;
		}
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
orchlua_write(lua_State *L)
{
	const char *buf;
	size_t bufsz, totalsz;
	ssize_t writesz;
	int fd;

	buf = luaL_checklstring(L, 1, &bufsz);
	fd = orchlua_cfg.termctl;
	totalsz = 0;
	while (totalsz < bufsz) {
		writesz = write(fd, &buf[totalsz], bufsz - totalsz);
		if (writesz == -1 && errno == EINTR) {
			continue;
		} else if (writesz == -1) {
			int err = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(err));
			return (2);
		}

		totalsz += writesz;
	}

	lua_pushnumber(L, totalsz);
	return (1);
}

/* Only true if we have read up to the eof, of course. */
static int
orchlua_eof(lua_State *L)
{

	lua_pushboolean(L, orchlua_cfg.termctl == -1);
	return (1);
}

static int
orchlua_exit(lua_State *L)
{

	exit(lua_toboolean(L, 1) ? 0 : 1);
}

static int
orchlua_time(lua_State *L)
{
	time_t t;

	t = time(NULL);
	lua_pushnumber(L, t);
	return (1);
}

#define	REG_SIMPLE(n)	{ #n, orchlua_ ## n }
static const struct luaL_Reg orchlib[] = {
	REG_SIMPLE(release),
	REG_SIMPLE(open),
	REG_SIMPLE(read),
	REG_SIMPLE(write),
	REG_SIMPLE(eof),
	REG_SIMPLE(exit),
	REG_SIMPLE(time),
	{ NULL, NULL },
};

void
orchlua_configure(struct orch_interp_cfg *cfg)
{

	orchlua_cfg = *cfg;
	orchlua_cfg.dirfd = -1;
}

/*
 * Available: functions above, orch_impl.script, orch_impl.script_root
 */
int
luaopen_orch(lua_State *L)
{
	char *script;
	int dirfd;

	luaL_newlib(L, orchlib);

	/* stdin */
	if (strcmp(orchlua_cfg.scriptf, "-") == 0) {
		script = "-";

		lua_pushnil(L);
	} else {
		char spath[PATH_MAX];
		char *fpath, *walker;
		const char *scriptroot;

		if ((fpath = realpath(orchlua_cfg.scriptf, &spath[0])) == NULL)
			err(1, "realpath");

		walker = strrchr(fpath, '/');
		if (walker != NULL) {
			*walker = '\0';
			scriptroot = fpath;
			script = walker + 1;
		} else {
			scriptroot = ".";
			script = fpath;
		}

		dirfd = open(scriptroot, O_DIRECTORY | O_PATH);
		if (dirfd == -1)
			err(1, "%s", fpath);

		orchlua_cfg.dirfd = dirfd;
		lua_pushstring(L, scriptroot);
	}

	/* script_root is on the stack from above. */
	lua_setfield(L, -2, "script_root");

	lua_pushstring(L, script);
	lua_setfield(L, -2, "script");

	return (1);
}
