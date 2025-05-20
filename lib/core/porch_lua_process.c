/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define	_FILE_OFFSET_BITS	64	/* Linux ino64 */

#include <sys/param.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "porch_lua.h"

#ifndef INFTIM
#define	INFTIM	(-1)
#endif

static void
porchlua_process_close_alarm(int signo __unused)
{
	/* XXX Ignored, just don't terminate us. */
}

static bool
porchlua_process_killed(struct porch_process *self, int *signo, bool hang)
{
	int flags = hang ? 0 : WNOHANG;

	assert(self->pid != 0);
	if (waitpid(self->pid, &self->status, flags) != self->pid)
		return (false);

	if (signo != NULL) {
		if (WIFSIGNALED(self->status))
			*signo = WTERMSIG(self->status);
		else
			*signo = 0;
	}

	self->pid = 0;

	return (true);
}

static void
porchlua_process_drain(lua_State *L, struct porch_process *self)
{

	/*
	 * Caller should have failed gracefully if the lua bits didn't set us up
	 * right.
	 */
	assert(lua_gettop(L) >= 2);

	/*
	 * Make a copy of the drain function, we may need to call it multiple times.
	 */
	self->draining = true;
	lua_pushvalue(L, -1);
	lua_call(L, 0, 0);
	self->draining = false;
}

static int
porchlua_process_chdir(lua_State *L)
{
	struct porch_ipc_msg *msg;
	struct porch_process *self;
	const char *dir;
	void *mdir;
	size_t dirsz;
	int error;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	dir = lua_tolstring(L, 2, &dirsz);
	if (!porch_ipc_okay(self->ipc)) {
		luaL_pushfail(L);
		lua_pushstring(L, "process already released");
		return (2);
	}

	msg = porch_ipc_msg_alloc(IPC_CHDIR, dirsz + 1, &mdir);
	if (msg == NULL)
		goto err;

	memcpy(mdir, dir, dirsz);
	error = porch_lua_ipc_send_acked_errno(L, self, msg, IPC_CHDIR_ACK);
	if (error < 0)
		goto err;
	else if (error > 0)
		return (error);

	lua_pushboolean(L, 1);
	return (1);
err:
	luaL_pushfail(L);
	lua_pushstring(L, strerror(errno));
	return (2);
}

static int
porchlua_process_close(lua_State *L)
{
	struct porch_process *self;
	pid_t wret;
	int sig;
	bool failed;

	failed = false;
	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	if (self->pid != 0 && porchlua_process_killed(self, &sig, false) &&
	    sig != 0) {
		luaL_pushfail(L);
		lua_pushfstring(L, "spawned process killed with signal '%d'", sig);
		return (2);
	}

	if (lua_gettop(L) < 2 || lua_isnil(L, 2)) {
		luaL_pushfail(L);
		lua_pushstring(L, "missing drain callback");
		return (2);
	}

	if (self->pid != 0) {
		struct sigaction sigalrm = {
			.sa_handler = porchlua_process_close_alarm,
		};

		sigaction(SIGALRM, &sigalrm, NULL);

		sig = SIGINT;
again:
		if (kill(self->pid, sig) == -1)
			warn("kill %d", sig);

		/* XXX Configurable? */
		if (sig != SIGKILL)
			alarm(5);
		if (sig == SIGKILL) {
			/*
			 * Once we've sent SIGKILL, we're tired of it; just drop the pty and
			 * anything that might've been added to the buffer after our SIGINT.
			 */
			if (self->termctl != -1) {
				close(self->termctl);
				self->termctl = -1;
			}
		} else {
			/*
			 * Some systems (e.g., Darwin/XNU) will wait for us to drain the tty
			 * when the controlling process exits.  We'll do that before we
			 * attempt to signal it, just in case.
			 */
			porchlua_process_drain(L, self);
		}

		wret = waitpid(self->pid, &self->status, 0);
		alarm(0);
		if (wret != self->pid) {
			failed = true;

			/* If asking nicely didn't work, just kill it. */
			if (sig != SIGKILL) {
				sig = SIGKILL;
				goto again;
			}
		}

		signal(SIGALRM, SIG_DFL);
		self->pid = 0;
	}

	porch_ipc_close(self->ipc);
	self->ipc = NULL;

	if (self->termctl != -1)
		close(self->termctl);
	self->termctl = -1;

	if (failed) {
		luaL_pushfail(L);
		lua_pushstring(L, "could not kill process with SIGINT");
		return (2);
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
porchlua_process_eof(lua_State *L)
{
	struct porch_process *self;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	lua_pushboolean(L, self->eof);
	return (1);
}

static int
porchlua_process_proxy_read(lua_State *L, int fd, int fn, bool *eof)
{
	char buf[4096];
	ssize_t readsz;

retry:
	readsz = read(fd, buf, sizeof(buf));
	if (readsz == -1 && errno == EINTR)
		goto retry;
	if (readsz == -1) {
		int serrno = errno;

		luaL_pushfail(L);
		lua_pushstring(L, strerror(serrno));
		return (2);
	}

	lua_pushvalue(L, fn);
	if (readsz == 0) {
		*eof = true;
		lua_pushnil(L);
	} else {
		lua_pushlstring(L, buf, readsz);
	}

	lua_call(L, 1, 0);
	return (0);
}

/*
 * proxy(file, outputfn, inputfn[, pulsefn]) -- signal that we're proxy'ing the
 * `file` stream into the process.  Lines read from `file` will be passed into
 * the `inputfn` for processing, and lines read from the process will be passed
 * into the `outputfn` for processing.  This function will put the `file`
 * stream into unbuffered mode.  The `pulsefn` will be invoked every second if
 * there is no input or output.
 */
static int
porchlua_process_proxy(lua_State *L)
{
	struct pollfd pfd[2];
	struct porch_process *self;
	luaL_Stream *p;
	FILE *inf;
	struct termios term;
	int infd, outfd, ready, ret, timeout;
	bool bailed, eof, has_pulse;

	bailed = eof = false;
	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	p = (luaL_Stream *)luaL_checkudata(L, 2, LUA_FILEHANDLE);
	luaL_checktype(L, 3, LUA_TFUNCTION);	/* outputfn */
	luaL_checktype(L, 4, LUA_TFUNCTION);	/* inputfn */
	has_pulse = lua_gettop(L) >= 5 && !lua_isnil(L, 5);
	if (has_pulse) {
		luaL_checktype(L, 5, LUA_TFUNCTION);	/* pulsefn */
		timeout = 1000;	/* pulsefn invoked every second. */
	} else {
		timeout = INFTIM;
	}

	inf = p->f;
	outfd = self->termctl;

	infd = dup(fileno(inf));
	if (infd == -1) {
		int serrno = errno;

		luaL_pushfail(L);
		lua_pushstring(L, strerror(serrno));
		return (2);
	}

	if (tcgetattr(infd, &term) == 0) {
		term.c_lflag &= ~(ICANON | ISIG);

		if (tcsetattr(infd, TCSANOW, &term) != 0) {
			int serrno = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(serrno));
			return (2);
		}
	} else if (errno != ENOTTY) {
		int serrno = errno;

		luaL_pushfail(L);
		lua_pushstring(L, strerror(serrno));
		return (2);
	}

	pfd[0].fd = outfd;
	pfd[0].events = POLLIN;

	pfd[1].fd = infd;
	pfd[1].events = POLLIN;

	while (!eof) {
		ready = poll(pfd, 2, timeout);
		if (ready == -1 && errno == EINTR)
			continue;
		if (ready == -1) {
			int serrno = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(serrno));
			return (2);
		}

		if (ready == 0) {
			assert(has_pulse);

			lua_pushvalue(L, 5);
			lua_call(L, 0, 1);
			bailed = !lua_toboolean(L, -1);
			lua_pop(L, 1);

			if (bailed)
				break;

			continue;
		}

		if ((pfd[0].revents & POLLIN) != 0) {
			ret = porchlua_process_proxy_read(L, outfd, 3, &eof);

			if (ret > 0)
				return (ret);

			if (eof) {
				if (self->pid == 0 ||
				    porchlua_process_killed(self, NULL, true)) {
					bailed = !WIFEXITED(self->status) ||
					    WEXITSTATUS(self->status) != 0;
				} else {
					bailed = true;
				}
			}
		}

		if ((pfd[1].revents & POLLIN) != 0) {
			ret = porchlua_process_proxy_read(L, infd, 4, &eof);
			if (ret > 0)
				return (ret);

			if (eof)
				bailed = true;
		} else if (eof) {
			/*
			 * Signal EOF to the input function if we didn't have
			 * any input, so that it can wrap up the script.
			 */
			lua_pushvalue(L, 4);
			lua_pushnil(L);
			lua_call(L, 1, 0);
		}
	}

	lua_pushboolean(L, !bailed);
	return (1);
}

/*
 * read(callback[, timeout]) -- returns true if we finished, false if we
 * hit EOF, or a fail, error pair otherwise.
 */
static int
porchlua_process_read(lua_State *L)
{
	char buf[LINE_MAX];
	fd_set rfd;
	struct porch_process *self;
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
	else
		start = now = timeout = 0;
	while ((tvp == NULL || now - start < timeout) && !self->error) {
		FD_SET(fd, &rfd);
		ret = select(fd + 1, &rfd, NULL, NULL, tvp);
		if (ret == -1 && errno == EINTR) {
			/*
			 * Rearm the timeout and go again; we'll just let the loop
			 * terminate with a negative tv.tv_sec if timeout seconds
			 * have elapsed.
			 */
			if (tvp != NULL) {
				now = time(NULL);
				tv.tv_sec = timeout - (now - start);
			}

			if (!self->draining)
				continue;

			/* Timeout */
			ret = 0;
		}

		if (ret == -1) {
			int err = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(err));
			return (2);
		} else if (ret == 0) {
			/* Timeout -- not the end of the world. */
			lua_pushboolean(L, 1);
			return (1);
		}

		/* Read it */
		readsz = read(fd, buf, sizeof(buf));

		/*
		 * Some platforms will return `0` when the slave side of a pty
		 * has gone away, while others will return -1 + EIO.  Convert
		 * the latter to the former.
		 */
		if (readsz == -1 && errno == EIO)
			readsz = 0;
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
			 * Callback should return true if it's done, false if it
			 * wants more.
			 */
			lua_call(L, nargs, 1);

			if (readsz == 0) {
				int signo;

				self->eof = true;

				assert(self->termctl >= 0);
				close(self->termctl);
				self->termctl = -1;

				if (!self->draining &&
				    porchlua_process_killed(self, &signo, false) &&
				    signo != 0) {
					luaL_pushfail(L);
					lua_pushfstring(L,
						"spawned process killed with signal '%d'", signo);
					return (2);
				}

				/*
				 * We need to be able to distinguish between a disaster
				 * scenario and possibly business as usual, so we'll
				 * return true if we hit EOF.  This lets us assert()
				 * on the return value and catch bad program exits.
				 */
				lua_pushboolean(L, 1);
				return (1);
			}

			if (lua_toboolean(L, -1))
				break;
		}
	}

	lua_pushboolean(L, 1);
	return (1);
}

static bool
porchlua_do_env(lua_State *L, struct porch_process *self, int index)
{
	struct porch_env *penv;
	struct porch_ipc_msg *msg;
	const char *setstr, *unsetstr;
	size_t envsz, setsz, unsetsz;
	bool clear = false;

	/* Push the "expand" method */
	lua_getfield(L, index, "expand");
	lua_pushvalue(L, index);
	lua_call(L, 1, 3);

	clear = lua_toboolean(L, -1);
	lua_pop(L, 1);

	/* Remaining: unset @ -1, set @ -2 */
	unsetstr = lua_tolstring(L, -1, &unsetsz);
	setstr = lua_tolstring(L, -2, &setsz);

	if (setsz > 0 && setstr[setsz - 1] != '\0') {
		luaL_pushfail(L);
		lua_pushstring(L, "Malformed env string");
		return (2);
	}

	assert(setsz != 0 || unsetsz != 0 || clear);

	envsz = sizeof(*penv) + setsz + unsetsz;
	msg = porch_ipc_msg_alloc(IPC_ENV_SETUP, envsz, (void **)&penv);
	if (msg == NULL) {
		luaL_pushfail(L);
		lua_pushstring(L, strerror(ENOMEM));
		return (2);
	}

	penv->clear = clear;
	penv->setsz = setsz;
	penv->unsetsz = unsetsz;
	if (setsz > 0)
		memcpy(&penv->envstr[0], setstr, setsz);
	if (unsetsz > 0)
		memcpy(&penv->envstr[setsz], unsetstr, unsetsz);
	lua_pop(L, 2);

	return (porch_lua_ipc_send_acked(L, self, msg, IPC_ENV_ACK));
}

static int
porchlua_process_release(lua_State *L)
{
	struct porch_process *self;
	int error;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);

	if (lua_istable(L, 2)) {
		int ret;

		ret = porchlua_do_env(L, self, 2);
		if (ret != 0)
			return (ret);
	}

	error = porch_release(self->ipc);
	porch_ipc_close(self->ipc);
	self->ipc = NULL;

	if (error != 0) {
		error = errno;

		luaL_pushfail(L);
		lua_pushstring(L, strerror(error));
		return (2);
	}

	self->released = true;

	lua_pushboolean(L, 1);
	return (1);
}

static int
porchlua_process_released(lua_State *L)
{
	struct porch_process *self;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	lua_pushboolean(L, self->released);
	return (1);
}

static int
porchlua_process_term_set(porch_ipc_t ipc __unused, struct porch_ipc_msg *msg,
    void *cookie)
{
	struct porch_term *term = cookie;
	struct termios *child_termios;
	struct termios *parent_termios = &term->term;
	size_t datasz;

	child_termios = porch_ipc_msg_payload(msg, &datasz);
	if (child_termios == NULL || datasz != sizeof(*child_termios)) {
		errno = EINVAL;
		return (-1);
	}

	memcpy(parent_termios, child_termios, sizeof(*child_termios));
	term->initialized = true;
	term->winsz_valid = false;

	return (0);
}

static int
porchlua_process_signal(lua_State *L)
{
	struct porch_process *self;
	int signal;

	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);

	/*
	 * We don't bother validating anything here in case they're wanting to
	 * use a signal that we don't know about.  kill(2) can validate this
	 * stuff better than we can.
	 */
	signal = luaL_checkinteger(L, 2);

	if (self->ipc != NULL) {
		/*
		 * We don't accept signaling processes before they're released
		 * for reasons, including because it doesn't seem useful to test
		 * how porch(1) itself handles signals.
		 */
		luaL_pushfail(L);
		lua_pushstring(L, "process not yet released");
		return (2);
	} else if (self->pid == 0) {
		luaL_pushfail(L);
		lua_pushstring(L, "process has already terminated");
	}

	assert(self->pid > 0);
	if (kill(self->pid, signal) != 0) {
		int serrno = errno;

		luaL_pushfail(L);
		lua_pushstring(L, strerror(serrno));
		return (2);
	}

	lua_pushboolean(L, 1);
	return (1);
}

static int
porchlua_process_term(lua_State *L)
{
	struct porch_term sterm;
	struct porch_ipc_msg *cmsg;
	struct porch_process *self;
	int error, retvals;

	retvals = 0;
	self = luaL_checkudata(L, 1, ORCHLUA_PROCESSHANDLE);
	if (!porch_ipc_okay(self->ipc)) {
		luaL_pushfail(L);
		lua_pushstring(L, "process already released");
		return (2);
	} else if (self->term != NULL) {
		luaL_pushfail(L);
		lua_pushstring(L, "process term already generated");
		return (2);
	}

	sterm.proc = self;
	sterm.initialized = false;
	porch_ipc_register(self->ipc, IPC_TERMIOS_SET, porchlua_process_term_set,
	    &sterm);

	/*
	 * The client is only responding to our messages up until we release, so
	 * there shouldn't be anything in the queue.  We'll just fire this off,
	 * and wait for a response to become ready.
	 */
	if ((error = porch_ipc_send_nodata(self->ipc,
	    IPC_TERMIOS_INQUIRY)) != 0) {
		error = errno;
		goto out;
	}

	if (porch_ipc_wait(self->ipc, NULL) == -1) {
		error = errno;
		goto out;
	}

	if (porch_ipc_recv(self->ipc, &cmsg) != 0) {
		error = errno;
		goto out;
	}

	if (cmsg != NULL) {
		luaL_pushfail(L);
		lua_pushfstring(L, "unexpected message type '%d'",
		    porch_ipc_msg_tag(cmsg));

		porch_ipc_msg_free(cmsg);
		cmsg = NULL;

		retvals = 2;
		goto out;
	} else if (!sterm.initialized) {
		luaL_pushfail(L);
		lua_pushstring(L, "unknown unexpected message received");
		retvals = 2;
		goto out;
	}

	retvals = porchlua_tty_alloc(L, &sterm, &self->term);

out:
	/* Deallocate the slot */
	porch_ipc_register(self->ipc, IPC_TERMIOS_SET, NULL, NULL);
	if (error != 0 && retvals == 0) {
		luaL_pushfail(L);
		lua_pushstring(L, strerror(error));

		retvals = 2;
	}

	return (retvals);
}

static int
porchlua_process_write(lua_State *L)
{
	struct porch_process *self;
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


#define	PROCESS_SIMPLE(n)	{ #n, porchlua_process_ ## n }
static const luaL_Reg porchlua_process[] = {
	PROCESS_SIMPLE(chdir),
	PROCESS_SIMPLE(close),
	PROCESS_SIMPLE(eof),
	PROCESS_SIMPLE(proxy),
	PROCESS_SIMPLE(read),
	PROCESS_SIMPLE(release),
	PROCESS_SIMPLE(released),
	PROCESS_SIMPLE(signal),
	PROCESS_SIMPLE(term),
	PROCESS_SIMPLE(write),
	{ NULL, NULL },
};

static const luaL_Reg porchlua_process_meta[] = {
	{ "__index", NULL },	/* Set during registration */
	{ "__gc", porchlua_process_close },
	{ "__close", porchlua_process_close },
	{ NULL, NULL },
};

void
porchlua_register_process_metatable(lua_State *L)
{
	luaL_newmetatable(L, ORCHLUA_PROCESSHANDLE);
	luaL_setfuncs(L, porchlua_process_meta, 0);

	luaL_newlibtable(L, porchlua_process);
	luaL_setfuncs(L, porchlua_process, 0);
	lua_setfield(L, -2, "__index");

	lua_pop(L, 1);
}

