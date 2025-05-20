/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define	_FILE_OFFSET_BITS	64	/* Linux ino64 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <unistd.h>

#include "porch_lua.h"

#ifdef __linux__
#define	CLOCK_REALTIME_FAST	CLOCK_REALTIME_COARSE
#endif

/* Fallback */
#ifndef CLOCK_REALTIME_FAST
#define	CLOCK_REALTIME_FAST	CLOCK_REALTIME
#endif

/* Not a huge deal if it's missing... */
#ifndef O_PATH
#define	O_PATH	0
#endif

#define	ORCHLUA_REGEXHANDLE	"porchlua_regex_t"

static struct porchlua_cfg {
	int			 dirfd;
	bool			 initialized;
} porchlua_cfg = {
	.dirfd = -1,
	.initialized = false,
};

static int porchlua_add_execpath(const char *);

/*
 * Not exported
 */
static int
porchlua_close(lua_State *L)
{
	luaL_Stream *p;
	int ret;

	p = (luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE);
	ret = fclose(p->f);
	return (luaL_fileresult(L, ret == 0, NULL));
}

static int
porchlua_open_init(const char *filename, const char **script, bool alter_path)
{

	assert(!porchlua_cfg.initialized);
	assert(porchlua_cfg.dirfd == -1);

	/* stdin */
	if (strcmp(filename, "-") == 0) {
		*script = filename;
	} else {
		char spath[PATH_MAX];
		char *fpath, *walker;
		const char *scriptroot;

		if ((fpath = realpath(filename, &spath[0])) == NULL)
			err(1, "realpath: %s", filename);

		walker = strrchr(fpath, '/');
		if (walker != NULL) {
			*walker = '\0';
			scriptroot = fpath;
			*script = strdup(walker + 1);
		} else {
			scriptroot = ".";
			*script = strdup(fpath);
		}

		if (*script == NULL)
			return (ENOMEM);

		if (alter_path)
			porchlua_add_execpath(scriptroot);

		porchlua_cfg.dirfd = open(scriptroot,
		    O_DIRECTORY | O_PATH | O_CLOEXEC);
		if (porchlua_cfg.dirfd == -1)
			err(1, "open: %s", fpath);
	}

	porchlua_cfg.initialized = true;
	return (0);
}

static int
porchlua_reset(lua_State *L)
{

	if (porchlua_cfg.initialized) {
		if (porchlua_cfg.dirfd >= 0) {
			close(porchlua_cfg.dirfd);
			porchlua_cfg.dirfd = -1;
		}

		porchlua_cfg.initialized = false;
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
porchlua_open(lua_State *L)
{
	luaL_Stream *op, *p;
	const char *filename, *script;
	int fd, rvals;
	bool alter_path;

	rvals = 1;
	if (lua_type(L, 1) == LUA_TUSERDATA) {
		filename = NULL;
		op = (luaL_Stream *)luaL_checkudata(L, 1, LUA_FILEHANDLE);
	} else {
		filename = luaL_checkstring(L, 1);
		op = NULL;
	}
	alter_path = lua_toboolean(L, 2);
	script = NULL;

	/*
	 * First open() sets up the sandbox state if we're running from a
	 * filename.  If we're not, we don't have a sandbox.  It would probably
	 * be nice to provide an API for the direct user to cope with that.
	 */
	if (filename != NULL) {
		if (!porchlua_cfg.initialized) {
			int error;

			error = porchlua_open_init(filename, &script, alter_path);
			if (error != 0) {
				luaL_pushfail(L);
				lua_pushstring(L, strerror(error));
				return (true);
			}
		} else if (porchlua_cfg.dirfd == -1) {
			luaL_pushfail(L);
			lua_pushstring(L,
			    "No sandbox granted (script opened from stdin)");
			return (2);
		} else {
			script = filename;
		}
	}

	fd = -1;
	if (filename != NULL) {
		if (porchlua_cfg.dirfd == -1) {
			assert(script == filename);
			fd = dup(STDIN_FILENO);
			if (fd == -1)
				return (luaL_fileresult(L, 0, "stdin"));
		}
	} else {
		assert(op != NULL);
		fd = dup(fileno(op->f));
		if (fd == -1)
			return (luaL_fileresult(L, 0, NULL));
	}

	p = (luaL_Stream *)lua_newuserdata(L, sizeof(*p));
	p->closef = &porchlua_close;
	p->f = NULL;
	luaL_setmetatable(L, LUA_FILEHANDLE);

	if (fd == -1)
		fd = openat(porchlua_cfg.dirfd, script, O_RDONLY | O_CLOEXEC);
	if (fd != -1)
		p->f = fdopen(fd, "r");

	/*
	 * Check for errors up front, to avoid clobbering errno.  We don't want
	 * to use luaL_fileresult() for success because this particular API
	 * returns a file handle.
	 */
	if (p->f == NULL) {
		rvals = luaL_fileresult(L, 0, filename);
	} else {
		rvals = 1;
		if (op != NULL) {
			off_t foff;

			/*
			 * Restore the stream position for the file that we
			 * copied, if we can.
			 */
			foff = ftello(op->f);
			if (foff == (off_t)-1)
				foff = 0;
			if (foff != 0)
				fseeko(p->f, foff, SEEK_SET);
		}
	}

	if (script != filename)
		free((void *)(uintptr_t)script);
	return (rvals);
}

static int
porchlua_regcomp(lua_State *L)
{
	const char *pattern;
	regex_t *regex;
	int error;

	pattern = luaL_checkstring(L, 1);

	regex = lua_newuserdata(L, sizeof(*regex));
	luaL_setmetatable(L, ORCHLUA_REGEXHANDLE);

	if ((error = regcomp(regex, pattern, REG_EXTENDED)) != 0) {
		char errbuf[64];

		(void)regerror(error, regex, errbuf, sizeof(errbuf));

		/* Pop the regex_t */
		lua_pop(L, 1);

		luaL_pushfail(L);
		lua_pushstring(L, errbuf);
		return (2);
	}

	return (1);
}

static int
porchlua_sleep(lua_State *L)
{
	lua_Number duration;
	struct timespec rtp;
	int ret;

	duration = luaL_checknumber(L, 1);
	rtp.tv_sec = floor(duration);
	rtp.tv_nsec = 1000000000 * (duration - rtp.tv_sec);

	/*
	 * We aren't guaranteeing anything about actual time delayed, just that
	 * we'll sleep at least the amount specified.  Let the timer run out.
	 */
	while ((ret = nanosleep(&rtp, &rtp)) == -1 && errno == EINTR) {
		continue;
	}

	if (ret == -1) {
		int serr = errno;

		luaL_pushfail(L);
		lua_pushstring(L, strerror(serr));
		return (2);
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
porchlua_time(lua_State *L)
{
	struct timespec tv;

	assert (clock_gettime(CLOCK_REALTIME_FAST, &tv) == 0);

	lua_pushnumber(L, tv.tv_sec);
	return (1);
}

static int
porchlua_child_error(porch_ipc_t ipc __unused, struct porch_ipc_msg *msg,
    void *cookie)
{
	struct porch_process *proc = cookie;
	const char *childstr;
	size_t datasz;

	childstr = porch_ipc_msg_payload(msg, &datasz);
	if (datasz != 0)
		fprintf(stderr, "CHILD ERROR: %.*s\n", (int)datasz, childstr);
	proc->error = true;
	return (0);
}

static int
porchlua_spawn(lua_State *L)
{
	struct porch_process *proc;
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

	/*
	 * Note that the one uservalue allowed by Lua < 5.4 is already consumed
	 * by the process buffer, so we can't allocate the porch_term for it up
	 * front.  We'll just allocate it if the module requests it, and will
	 * not attach it to the proc -- porch.lua will just have to manage its
	 * lifetime appropriately.
	 */
	proc = lua_newuserdata(L, sizeof(*proc));
	proc->L = L;
	proc->term = NULL;
	proc->status = 0;
	proc->pid = 0;
	proc->buffered = proc->eof = proc->released = proc->draining = false;
	proc->error = false;

	luaL_setmetatable(L, ORCHLUA_PROCESSHANDLE);

	if (porch_spawn(argc, argv, proc, &porchlua_child_error) != 0) {
		int serrno = errno;

		free(argv);

		luaL_pushfail(L);
		lua_pushstring(L, strerror(serrno));
		return (2);
	}

	free(argv);

	return (1);
}

#define	REG_SIMPLE(n)	{ #n, porchlua_ ## n }
static const struct luaL_Reg porchlib[] = {
	REG_SIMPLE(open),
	REG_SIMPLE(regcomp),
	REG_SIMPLE(reset),
	REG_SIMPLE(sleep),
	REG_SIMPLE(time),
	REG_SIMPLE(spawn),
	{ NULL, NULL },
};

static void
porchlua_install_signals(lua_State *L)
{
	const char * const *signames;
	size_t signalcnt;

	signalcnt = 0;
	signames = porch_signames(&signalcnt);
	assert(signalcnt != 0);

	lua_newtable(L);
	for (size_t signo = 0; signo < signalcnt; signo++) {
		const char *signame;

		/*
		 * Signal #0 is special and we never map a name for it; for all
		 * other signals, we'll map the name to the signal number.  Any
		 * signals that don't have names are expected to be papered over
		 * by the .orch script or lib user using their literal values
		 * and losing the convenience of names.
		 */
		signame = signames[signo];
		if (signo == 0 || signame == NULL)
			continue;

		/*
		 * I suspect most platforms won't have returned a SIG prefix,
		 * but it costs little to be flexible; this is certainly not
		 * a hot path.
		 */
		if (strncmp(signame, "SIG", 3) != 0)
			lua_pushfstring(L, "SIG%s", signame);
		else
			lua_pushstring(L, signame);
		lua_pushinteger(L, signo);
		lua_rawset(L, -3);
	}

	/* core.signals */
	lua_setfield(L, -2, "signals");
}

static int
porchlua_regex_error(lua_State *L, regex_t *self, int error)
{
	char errbuf[64];

	(void)regerror(error, self, errbuf, sizeof(errbuf));

	luaL_pushfail(L);
	lua_pushstring(L, errbuf);
	return (2);
}

static int
porchlua_regex_find(lua_State *L)
{
	const char *subject;
	regex_t *self;
	regmatch_t match;
	int error;

	self = luaL_checkudata(L, 1, ORCHLUA_REGEXHANDLE);
	subject = luaL_checkstring(L, 2);

	error = regexec(self, subject, 1, &match, 0);
	if (error != 0) {
		if (error == REG_NOMATCH) {
			lua_pushnil(L);
			return (1);
		}

		return (porchlua_regex_error(L, self, error));
	}

	/*
	 * Lua's strings are one-indexed, so we bump rm_so by 1.  rm_eo is
	 * actually the the character just *after* the match, so we'll just take
	 * that as-is rather than - 1 + 1.
	 */
	lua_pushnumber(L, match.rm_so + 1);
	lua_pushnumber(L, match.rm_eo);
	return (2);
}

static int
porchlua_regex_close(lua_State *L)
{
	regex_t *self;

	self = luaL_checkudata(L, 1, ORCHLUA_REGEXHANDLE);
	regfree(self);
	return (0);
}

#define	REGEX_SIMPLE(n)	{ #n, porchlua_regex_ ## n }
static const luaL_Reg porchlua_regex[] = {
	REGEX_SIMPLE(find),
	{ NULL, NULL },
};

static const luaL_Reg porchlua_regex_meta[] = {
	{ "__index", NULL },	/* Set during registration */
	{ "__gc", porchlua_regex_close },
	{ "__close", porchlua_regex_close },
	{ NULL, NULL },
};

static void
porchlua_register_regex_metatable(lua_State *L)
{
	luaL_newmetatable(L, ORCHLUA_REGEXHANDLE);
	luaL_setfuncs(L, porchlua_regex_meta, 0);

	luaL_newlibtable(L, porchlua_regex);
	luaL_setfuncs(L, porchlua_regex, 0);
	lua_setfield(L, -2, "__index");

	lua_pop(L, 1);
}

static int
porchlua_add_execpath(const char *path)
{
	const char *curpath;
	char *newpath;
	int error;

	error = 0;
	curpath = getenv("PATH");
	/* Odd, but nothing to do but just add it... */
	if (curpath == NULL)
		return (setenv("PATH", path, 1) == 0 ? 0 : errno);

	newpath = NULL;
	if (asprintf(&newpath, "%s:%s", path, curpath) == -1)
		return (ENOMEM);

	if (setenv("PATH", newpath, 1) != 0)
		error = errno;

	free(newpath);
	return (error);
}

/*
 * Available: functions above, porch_impl.script, porch_impl.script_root
 */
int
luaopen_porch_core(lua_State *L)
{

	luaL_newlib(L, porchlib);

	porchlua_install_signals(L);
	porchlua_setup_tty(L);

	porchlua_register_process_metatable(L);
	porchlua_register_regex_metatable(L);

	return (1);
}
