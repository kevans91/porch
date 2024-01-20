/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>

#include <stdbool.h>
#include <termios.h>

#include <lua.h>
#include <lauxlib.h>

/* We only support Lua 5.2+ */

/* Introduced in Lua 5.4 */
#ifndef luaL_pushfail
#define	luaL_pushfail(L)	lua_pushnil(L)
#endif

enum orch_ipc_tag {
	IPC_RELEASE = 1,	/* Bidrectional */
	IPC_ERROR,		/* Child -> Parent */
	IPC_TERMIOS_INQUIRY,	/* Parent -> Child */
	IPC_TERMIOS_SET,	/* Bidirectional */
	IPC_TERMIOS_ACK,	/* Child -> Parent */
	IPC_LAST,
};

struct orch_ipc_header {
	size_t			 size;
	enum orch_ipc_tag	 tag;
};

struct orch_ipc_msg {
	struct orch_ipc_header		 hdr;
	_Alignas(max_align_t) unsigned char	 data[];
};

struct orch_term;

struct orch_process {
	lua_State		*L;
	struct orch_term	*term;
	int			 cmdsock;
	pid_t			 pid;
	int			 status;
	int			 termctl;
	bool			 raw;
	bool			 released;
	bool			 eof;
	bool			 buffered;
	bool			 error;
};

struct orch_term {
	struct termios		term;
	bool			initialized;
};

struct orchlua_tty_cntrl {
	int		 cntrl_idx;
	const char	*cntrl_name;
	int		 cntrl_flags;
};

struct orchlua_tty_mode {
	int		 mode_mask;
	const char	*mode_name;
};

#define	CNTRL_CANON	0x01
#define	CNTRL_NCANON	0x02
#define	CNTRL_BOTH	0x03
#define	CNTRL_LITERAL	0x04

/* orch_ipc.c */
typedef int (orch_ipc_handler)(struct orch_ipc_msg *, void *);
int orch_ipc_close(void);
void orch_ipc_open(int);
bool orch_ipc_okay(void);
int orch_ipc_recv(struct orch_ipc_msg **);
int orch_ipc_register(enum orch_ipc_tag, orch_ipc_handler *, void *);
int orch_ipc_send(struct orch_ipc_msg *);
int orch_ipc_wait(bool *);

/* orch_spawn.c */
int orch_release(void);
int orch_spawn(int, const char *[], struct orch_process *);

/* orch_tty.c */
int orchlua_setup_tty(lua_State *);
int orchlua_tty_alloc(lua_State *, const struct orch_term *,
    struct orch_term **);

extern const struct orchlua_tty_cntrl orchlua_cntrl_chars[];
extern const struct orchlua_tty_mode orchlua_input_modes[];
extern const struct orchlua_tty_mode orchlua_output_modes[];
extern const struct orchlua_tty_mode orchlua_cntrl_modes[];
extern const struct orchlua_tty_mode orchlua_local_modes[];
