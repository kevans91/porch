/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdbool.h>

#include <lua.h>

#define	ORCHLUA_MODNAME	"orch_impl"

struct orch_interp_cfg {
	const char		*scriptf;
	int			 cmdsock;
	int			 termctl;
	int			 dirfd;
	bool			 released;
};

/* orch_interp.c */
int orch_interp(const char *, int, int);

/* orch_lua.c */
void orchlua_configure(struct orch_interp_cfg *);
int luaopen_orch(lua_State *);
