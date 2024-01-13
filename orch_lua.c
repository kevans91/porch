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

#define	ORCHLUA_PROCESSHANDLE	"orchlua_process"

static struct orch_interp_cfg orchlua_cfg;

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
		fd = openat(orchlua_cfg.dirfd, filename, O_RDONLY | O_CLOEXEC);
	if (fd == -1)
		return (luaL_fileresult(L, 0, filename));

	p->f = fdopen(fd, "r");
	if (p->f == NULL)
		return (luaL_fileresult(L, 0, filename));

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
	struct timespec tv;

	assert (clock_gettime(CLOCK_REALTIME_FAST, &tv) == 0);

	lua_pushnumber(L, tv.tv_sec);
	return (1);
}

static int
orchlua_spawn(lua_State *L)
{
	struct orch_process *proc;
	const char **argv;
	int argc;

	if (lua_gettop(L) == 0) {
		luaL_pushfail(L);
		lua_pushstring(L, "No command specified to spawn");
		return (2);
	}

	/*
	 * The script can table.unpack its args, so we'll expect all strings even if
	 * they choose to build it up via table.
	 */
	argc = lua_gettop(L);
	argv = calloc(argc + 1, sizeof(*argv));

	for (int i = 0; i < argc; i++) {
		argv[i] = lua_tostring(L, i + 1);
		if (argv[i] == NULL) {
			free(argv);
			luaL_pushfail(L);
			lua_pushfstring(L, "Argument at index %d not a string", i + 1);
			return (2);
		}
	}

	proc = lua_newuserdata(L, sizeof(*proc));
	proc->eof = proc->released = false;

	luaL_setmetatable(L, ORCHLUA_PROCESSHANDLE);
	orch_spawn(argc, argv, proc);
	return (1);
}

#define	REG_SIMPLE(n)	{ #n, orchlua_ ## n }
static const struct luaL_Reg orchlib[] = {
	REG_SIMPLE(open),
	REG_SIMPLE(exit),
	REG_SIMPLE(time),
	REG_SIMPLE(spawn),
	{ NULL, NULL },
};

/*
 * read(callback[, timeout]) -- returns true if we finished, false if we
 * hit EOF, or a fail, error pair otherwise.
 */
static int
orchlua_process_read(lua_State *L)
{
	char buf[LINE_MAX];
	fd_set rfd;
	struct orch_process *self;
	struct timeval tv, *tvp;
	time_t start, now;
	ssize_t readsz;
	int fd, ret;
	lua_Number timeout;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (lua_gettop(L) > 2) {
		timeout = luaL_checknumber(L, 3);
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

	fd = self->termctl;
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
			lua_settop(L, 3);
			lua_copy(L, -2, -1);

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
				self->eof = true;

				/* Don't care about the return value if we hit EOF. */
				lua_pushboolean(L, 0);

				close(self->termctl);
				self->termctl = -1;
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
orchlua_process_write(lua_State *L)
{
	struct orch_process *self;
	const char *buf;
	size_t bufsz, totalsz;
	ssize_t writesz;
	int fd;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	buf = luaL_checklstring(L, 2, &bufsz);
	fd = self->termctl;
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


static int
orchlua_process_release(lua_State *L)
{
	struct orch_process *self;
	ssize_t wsz;
	int buf = 0, err;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	if ((wsz = write(self->cmdsock, &buf, sizeof(buf))) != sizeof(buf)) {
		err = errno;
		luaL_pushfail(L);
		if (wsz < 0)
			lua_pushstring(L, strerror(err));
		else
			lua_pushstring(L, "cmd socket closed");
		return (2);
	}

	self->released = true;

	lua_pushboolean(L, 1);
	return (1);
}

static int
orchlua_process_released(lua_State *L)
{
	struct orch_process *self;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	lua_pushboolean(L, self->released);
	return (1);
}

static int
orchlua_process_eof(lua_State *L)
{
	struct orch_process *self;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	lua_pushboolean(L, self->eof);
	return (1);
}

static int
orchlua_process_gc(lua_State *L __unused)
{

	/* XXX reap child */
	fprintf(stderr, "GC Called\n");
	return (0);
}

#define	PROCESS_SIMPLE(n)	{ #n, orchlua_process_ ## n }
static const luaL_Reg orchlua_process[] = {
	PROCESS_SIMPLE(read),
	PROCESS_SIMPLE(write),
	PROCESS_SIMPLE(release),
	PROCESS_SIMPLE(released),
	PROCESS_SIMPLE(eof),
	{ NULL, NULL },
};

static const luaL_Reg orchlua_process_meta[] = {
	{ "__index", NULL },	/* Set during registration */
	{ "__gc", orchlua_process_gc },
	{ "__close", orchlua_process_gc },
	{ NULL, NULL },
};

static void
register_process_metatable(lua_State *L)
{
	luaL_newmetatable(L, ORCHLUA_PROCESSHANDLE);
	luaL_setfuncs(L, orchlua_process_meta, 0);

	luaL_newlibtable(L, orchlua_process);
	luaL_setfuncs(L, orchlua_process, 0);
	lua_setfield(L, -2, "__index");

	lua_pop(L, 1);
}

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
	const char *script;
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

		dirfd = open(scriptroot, O_DIRECTORY | O_PATH | O_CLOEXEC);
		if (dirfd == -1)
			err(1, "%s", fpath);

		orchlua_cfg.dirfd = dirfd;
		lua_pushstring(L, scriptroot);
	}

	/* script_root is on the stack from above. */
	lua_setfield(L, -2, "script_root");

	lua_pushstring(L, script);
	lua_setfield(L, -2, "script");

	lua_createtable(L, orchlua_cfg.argc, 0);
	for (int i = 0; i < orchlua_cfg.argc; i++) {
		lua_pushstring(L, orchlua_cfg.argv[i]);
		lua_rawseti(L, -2, i + 1);
	}
	lua_setglobal(L, "arg");

	register_process_metatable(L);

	return (1);
}
