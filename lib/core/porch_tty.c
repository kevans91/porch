/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "porch.h"
#include "porch_lib.h"

#define	ORCHLUA_TERMHANDLE	"porch_term"

#define	CNTRL_ENTRY(c, m)	{ c, #c, m }
const struct porchlua_tty_cntrl porchlua_cntrl_chars[] = {
	CNTRL_ENTRY(VEOF,	CNTRL_CANON),
	CNTRL_ENTRY(VEOL,	CNTRL_CANON),
	CNTRL_ENTRY(VERASE,	CNTRL_CANON),
	CNTRL_ENTRY(VINTR,	CNTRL_BOTH),
	CNTRL_ENTRY(VKILL,	CNTRL_CANON),
	CNTRL_ENTRY(VMIN,	CNTRL_NCANON | CNTRL_LITERAL),
	CNTRL_ENTRY(VQUIT,	CNTRL_BOTH),
	CNTRL_ENTRY(VSUSP,	CNTRL_BOTH),
	CNTRL_ENTRY(VTIME,	CNTRL_NCANON | CNTRL_LITERAL),
	CNTRL_ENTRY(VSTART,	CNTRL_BOTH),
	CNTRL_ENTRY(VSTOP,	CNTRL_BOTH),
#ifdef VSTATUS
	CNTRL_ENTRY(VSTATUS,	CNTRL_CANON),
#endif
	{ 0, NULL, 0 },
};

/*
 * I only care about local modes personally, but the other tables are present to
 * avoid putting up any barriers if more modes are useful to someone else.
 */
#define	MODE_ENTRY(c)		{ c, #c }
const struct porchlua_tty_mode porchlua_input_modes[] = {
	{ 0, NULL },
};

const struct porchlua_tty_mode porchlua_output_modes[] = {
	{ 0, NULL },
};

const struct porchlua_tty_mode porchlua_cntrl_modes[] = {
	{ 0, NULL },
};

const struct porchlua_tty_mode porchlua_local_modes[] = {
	MODE_ENTRY(ECHO),
	MODE_ENTRY(ECHOE),
	MODE_ENTRY(ECHOK),
	MODE_ENTRY(ECHONL),
	MODE_ENTRY(ICANON),
	MODE_ENTRY(IEXTEN),
	MODE_ENTRY(ISIG),
	MODE_ENTRY(NOFLSH),
	MODE_ENTRY(TOSTOP),
	{ 0, NULL },
};

static void
porchlua_term_fetch_cc(lua_State *L, struct termios *term)
{
	const struct porchlua_tty_cntrl *iter;
	cc_t cc;

	for (iter = &porchlua_cntrl_chars[0]; iter->cntrl_name != NULL; iter++) {
		cc = term->c_cc[iter->cntrl_idx];

		if ((iter->cntrl_flags & CNTRL_LITERAL) != 0)
			lua_pushinteger(L, cc);
		else if (cc == _POSIX_VDISABLE)
			lua_pushstring(L, "");
		else if (cc == 0177)
			lua_pushstring(L, "^?");
		else
			lua_pushfstring(L, "^%c", cc + 0x40);

		lua_setfield(L, -2, iter->cntrl_name);
	}
}

static int
porchlua_term_fetch(lua_State *L)
{
	struct porch_term *self;
	const char *which;
	int retvals = 0, top;

	self = luaL_checkudata(L, 1, ORCHLUA_TERMHANDLE);
	if ((top = lua_gettop(L)) < 2) {
		lua_pushnil(L);
		return (1);
	}

	for (int i = 1; i < top; i++) {
		which = luaL_checkstring(L, i + 1);

		if (strcmp(which, "iflag") == 0) {
			lua_pushnumber(L, self->term.c_iflag);
		} else if (strcmp(which, "oflag") == 0) {
			lua_pushnumber(L, self->term.c_oflag);
		} else if (strcmp(which, "cflag") == 0) {
			lua_pushnumber(L, self->term.c_cflag);
		} else if (strcmp(which, "lflag") == 0) {
			lua_pushnumber(L, self->term.c_lflag);
		} else if (strcmp(which, "cc") == 0) {
			lua_newtable(L);
			porchlua_term_fetch_cc(L, &self->term);
		} else {
			lua_pushnil(L);
		}

		retvals++;
	}

	return (retvals);
}

static int
porchlua_term_update_cc(lua_State *L, struct termios *term)
{
	const struct porchlua_tty_cntrl *iter;
	int type;
	cc_t cc;

	for (iter = &porchlua_cntrl_chars[0]; iter->cntrl_name != NULL; iter++) {
		type = lua_getfield(L, -1, iter->cntrl_name);
		if (type == LUA_TNIL) {
			lua_pop(L, 1);
			continue;
		}

		if ((iter->cntrl_flags & CNTRL_LITERAL) != 0) {
			int valid;

			cc = lua_tonumberx(L, -1, &valid);
			if (!valid) {
				luaL_pushfail(L);
				lua_pushfstring(L, "expected number for cc '%s'",
				    iter->cntrl_name);
				return (2);
			}
		} else {
			const char *str;
			size_t len;

			if (type != LUA_TSTRING) {
				luaL_pushfail(L);
				lua_pushfstring(L, "expected string for cc '%s'",
				    iter->cntrl_name);
				return (2);
			}

			str = lua_tolstring(L, -1, &len);
			if (len == 0) {
				cc = _POSIX_VDISABLE;
			} else if (len != 2 || str[0] != '^') {
				luaL_pushfail(L);
				lua_pushfstring(L,
				    "malformed value for cc '%s': %s",
				    iter->cntrl_name, str);
				return (2);
			} else if (str[1] != '?' &&
			    (str[1] < 0x40 || str[1] > 0x5f)) {
				luaL_pushfail(L);
				lua_pushfstring(L,
				    "cntrl char for cc '%s' out of bounds: %c",
				    iter->cntrl_name, str[1]);
				return (2);
			} else {
				if (str[1] == '?')
					cc = 0177;
				else
					cc = str[1] - 0x40;
			}
		}

		term->c_cc[iter->cntrl_idx] = cc;
		lua_pop(L, 1);
	}

	return (0);
}

static int
porchlua_term_update(lua_State *L)
{
	const char *fields[] = { "iflag", "oflag", "lflag", "cc", NULL };
	struct porch_term *self;
	struct porch_ipc_msg *msg;
	const char **fieldp, *field;
	struct termios *msgterm, updated;
	int error, type, valid;

	self = luaL_checkudata(L, 1, ORCHLUA_TERMHANDLE);
	if (!lua_istable(L, 2)) {
		luaL_pushfail(L);
		lua_pushstring(L, "argument #2 must be table of fields to update");
		return (2);
	}

	lua_settop(L, 2);

	updated = self->term;
	for (fieldp = &fields[0]; *fieldp != NULL; fieldp++) {
		field = *fieldp;

		type = lua_getfield(L, -1, field);
		if (type == LUA_TNIL) {
			lua_pop(L, 1);
			continue;
		}

		if (strcmp(field, "iflag") == 0) {
			updated.c_iflag = lua_tonumberx(L, -1, &valid);
			if (!valid) {
				luaL_pushfail(L);
				lua_pushstring(L, "iflag must be a numeric mask");
				return (2);
			}
		} else if (strcmp(field, "oflag") == 0) {
			updated.c_oflag = lua_tonumberx(L, -1, &valid);
			if (!valid) {
				luaL_pushfail(L);
				lua_pushstring(L, "oflag must be a numeric mask");
				return (2);
			}
		} else if (strcmp(field, "cflag") == 0) {
			updated.c_cflag = lua_tonumberx(L, -1, &valid);
			if (!valid) {
				luaL_pushfail(L);
				lua_pushstring(L, "cflag must be a numeric mask");
				return (2);
			}
		} else if (strcmp(field, "lflag") == 0) {
			updated.c_lflag = lua_tonumberx(L, -1, &valid);
			if (!valid) {
				luaL_pushfail(L);
				lua_pushstring(L, "lflag must be a numeric mask");
				return (2);
			}
		} else if (strcmp(field, "cc") == 0) {
			if (type != LUA_TTABLE) {
				luaL_pushfail(L);
				lua_pushstring(L, "cc must be a table of characters to remap");
				return (2);
			}

			if ((error = porchlua_term_update_cc(L, &updated)) != 0)
				return (error);
		}

		lua_pop(L, 1);
	}

	self->term = updated;

	msg = porch_ipc_msg_alloc(IPC_TERMIOS_SET, sizeof(self->term),
	    (void **)&msgterm);
	if (msg == NULL) {
		luaL_pushfail(L);
		lua_pushstring(L, strerror(ENOMEM));
		return (2);
	}

	memcpy(msgterm, &self->term, sizeof(self->term));
	error = porch_lua_ipc_send_acked(L, self->proc, msg, IPC_TERMIOS_ACK);
	if (error != 0)
		return (error);

	lua_pushboolean(L, 1);
	return (1);
}

static int
porchlua_term_size(lua_State *L)
{
	struct porch_term *self;
	lua_Number val;
	bool fetching;

	self = luaL_checkudata(L, 1, ORCHLUA_TERMHANDLE);
	if (!self->winsz_valid) {
		if (ioctl(self->proc->termctl, TIOCGWINSZ, &self->winsz) != 0) {
			int error = errno;

			luaL_pushfail(L);
			lua_pushstring(L, strerror(error));
			return (2);
		}

		self->winsz_valid = true;
	}

	/*
	 * If size doesn't have both width and height arguments, it simply
	 * return the current size.
	 */
	fetching = lua_isnoneornil(L, 2) && lua_isnoneornil(L, 3);
	if (!fetching) {
			if (!lua_isnoneornil(L, 2)) {
				val = luaL_checknumber(L, 2);
				if (val < 0 || val > USHRT_MAX) {
					luaL_pushfail(L);
					lua_pushfstring(L, "width out of bounds: %llu\n",
					    (uint64_t)val);
					return (2);
				}

				self->winsz.ws_col = val;
			}

			if (!lua_isnoneornil(L, 3)) {
				val = luaL_checknumber(L, 3);
				if (val < 0 || val > USHRT_MAX) {
					luaL_pushfail(L);
					lua_pushfstring(L, "height out of bounds: %llu\n",
					    (uint64_t)val);
					return (2);
				}

				self->winsz.ws_row = val;
			}

			if (ioctl(self->proc->termctl, TIOCSWINSZ, &self->winsz) != 0) {
				int error = errno;

				luaL_pushfail(L);
				lua_pushstring(L, strerror(error));
				return (2);
			}
	}

	lua_pushnumber(L, self->winsz.ws_col);
	lua_pushnumber(L, self->winsz.ws_row);

	return (2);
}

#define	ORCHTERM_SIMPLE(n) { #n, porchlua_term_ ## n }
static const luaL_Reg porchlua_term[] = {
	ORCHTERM_SIMPLE(fetch),
	ORCHTERM_SIMPLE(update),
	ORCHTERM_SIMPLE(size),
	{ NULL, NULL },
};

static const luaL_Reg porchlua_term_meta[] = {
	{ "__index", NULL },	/* Set during registratino */
	/* Nothing to __gc / __close just yet. */
	{ NULL, NULL },
};

static void
register_term_metatable(lua_State *L)
{
	luaL_newmetatable(L, ORCHLUA_TERMHANDLE);
	luaL_setfuncs(L, porchlua_term_meta, 0);

	luaL_newlibtable(L, porchlua_term);
	luaL_setfuncs(L, porchlua_term, 0);
	lua_setfield(L, -2, "__index");

	lua_pop(L, 1);
}

static void
porchlua_tty_add_cntrl(lua_State *L, const char *name,
    const struct porchlua_tty_cntrl *mcntrl)
{
	const struct porchlua_tty_cntrl *iter;

	lua_newtable(L);
	for (iter = mcntrl; iter->cntrl_name != NULL; iter++) {
		lua_pushboolean(L, 1);
		lua_setfield(L, -2, iter->cntrl_name);
	}

	lua_setfield(L, -2, name);
}

static void
porchlua_tty_add_modes(lua_State *L, const char *name,
    const struct porchlua_tty_mode *mtable)
{
	const struct porchlua_tty_mode *iter;

	lua_newtable(L);
	for (iter = mtable; iter->mode_mask != 0; iter++) {
		lua_pushinteger(L, iter->mode_mask);
		lua_setfield(L, -2, iter->mode_name);
	}

	lua_setfield(L, -2, name);
}

int
porchlua_setup_tty(lua_State *L)
{

	/* Module is on the stack. */
	lua_newtable(L);

	/* tty.iflag, tty.oflag, tty.cflag, tty.lflag */
	porchlua_tty_add_modes(L, "iflag", &porchlua_input_modes[0]);
	porchlua_tty_add_modes(L, "oflag", &porchlua_output_modes[0]);
	porchlua_tty_add_modes(L, "cflag", &porchlua_cntrl_modes[0]);
	porchlua_tty_add_modes(L, "lflag", &porchlua_local_modes[0]);

	/* tty.cc */
	porchlua_tty_add_cntrl(L, "cc", &porchlua_cntrl_chars[0]);

	lua_setfield(L, -2, "tty");

	register_term_metatable(L);

	return (1);
}

int
porchlua_tty_alloc(lua_State *L, const struct porch_term *copy,
    struct porch_term **otermp)
{
	struct porch_term *term;

	term = lua_newuserdata(L, sizeof(*term));
	memcpy(term, copy, sizeof(*copy));

	*otermp = term;

	luaL_setmetatable(L, ORCHLUA_TERMHANDLE);
	return (1);
}
