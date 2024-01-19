/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>

#include <stdbool.h>

#include <lua.h>

#define	ORCHLUA_MODNAME	"orch_impl"

#ifndef __unused
#define	__unused	__attribute__((unused))
#endif

struct orch_interp_cfg {
	const char		*scriptf;
	int			 dirfd;
	int			 argc;
	const char *const	*argv;
};

struct orch_process {
	int			 cmdsock;
	pid_t			 pid;
	int			 status;
	int			 termctl;
	bool			 raw;
	bool			 released;
	bool			 eof;
	bool			 buffered;
};

/* orch.c */
int orch_spawn(int, const char *[], struct orch_process *);

/* orch_interp.c */
int orch_interp(const char *, const char *, int, const char * const []);

/* orch_lua.c */
void orchlua_configure(struct orch_interp_cfg *);
int luaopen_orch(lua_State *);

/* orch_compat.c */
#ifdef __linux__
size_t strlcpy(char * __restrict dst, const char * __restrict src, size_t dsize);
size_t strlcat(char * __restrict dst, const char * __restrict src, size_t dsize);
#endif
#if defined(__linux__) || defined(__APPLE__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
int tcsetsid(int tty, int sess);
#endif
