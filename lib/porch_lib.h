/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>
#include <sys/ioctl.h>

#include <stdbool.h>
#include <termios.h>

#include <lua.h>
#include <lauxlib.h>

/* We only support Lua 5.3+ */

/* Introduced in Lua 5.4 */
#ifndef luaL_pushfail
#define	luaL_pushfail(L)	lua_pushnil(L)
#endif

struct porch_ipc_msg;
typedef struct porch_ipc *porch_ipc_t;

struct porch_term;

enum porch_ipc_tag {
	IPC_NOXMIT = 0,
	IPC_RELEASE,		/* Bidrectional */
	IPC_ERROR,		/* Child -> Parent */
	IPC_TERMIOS_INQUIRY,	/* Parent -> Child */
	IPC_TERMIOS_SET,	/* Bidirectional */
	IPC_TERMIOS_ACK,	/* Child -> Parent */
	IPC_LAST,
};

struct porch_process {
	lua_State		*L;
	struct porch_term	*term;
	porch_ipc_t		 ipc;
	int			 cmdsock;
	pid_t			 pid;
	int			 status;
	int			 termctl;
	bool			 raw;
	bool			 released;
	bool			 eof;
	bool			 buffered;
	bool			 error;
	bool			 draining;
};

struct porch_term {
	struct termios		term;
	struct winsize		winsz;
	struct porch_process	*proc;
	bool			initialized;
	bool			winsz_valid;
};

struct porchlua_tty_cntrl {
	int		 cntrl_idx;
	const char	*cntrl_name;
	int		 cntrl_flags;
};

struct porchlua_tty_mode {
	int		 mode_mask;
	const char	*mode_name;
};

#define	CNTRL_CANON	0x01
#define	CNTRL_NCANON	0x02
#define	CNTRL_BOTH	0x03
#define	CNTRL_LITERAL	0x04

/* porch_ipc.c */
typedef int (porch_ipc_handler)(porch_ipc_t, struct porch_ipc_msg *, void *);
int porch_ipc_close(porch_ipc_t);
porch_ipc_t porch_ipc_open(int);
bool porch_ipc_okay(porch_ipc_t);
int porch_ipc_recv(porch_ipc_t, struct porch_ipc_msg **);
struct porch_ipc_msg *porch_ipc_msg_alloc(enum porch_ipc_tag, size_t, void **);
void *porch_ipc_msg_payload(struct porch_ipc_msg *, size_t *);
enum porch_ipc_tag porch_ipc_msg_tag(struct porch_ipc_msg *);
void porch_ipc_msg_free(struct porch_ipc_msg *);
int porch_ipc_register(porch_ipc_t, enum porch_ipc_tag, porch_ipc_handler *, void *);
int porch_ipc_send(porch_ipc_t, struct porch_ipc_msg *);
int porch_ipc_send_nodata(porch_ipc_t, enum porch_ipc_tag);
int porch_ipc_wait(porch_ipc_t, bool *);

/* porch_spawn.c */
int porch_release(porch_ipc_t);
int porch_spawn(int, const char *[], struct porch_process *, porch_ipc_handler *);

/* porch_tty.c */
int porchlua_setup_tty(lua_State *);
int porchlua_tty_alloc(lua_State *, const struct porch_term *,
    struct porch_term **);

extern const struct porchlua_tty_cntrl porchlua_cntrl_chars[];
extern const struct porchlua_tty_mode porchlua_input_modes[];
extern const struct porchlua_tty_mode porchlua_output_modes[];
extern const struct porchlua_tty_mode porchlua_cntrl_modes[];
extern const struct porchlua_tty_mode porchlua_local_modes[];
