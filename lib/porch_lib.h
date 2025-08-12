/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>
#include <sys/ioctl.h>

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>

#include "porch_lib_signals.h"

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
	IPC_TERMIOS_SET,	/* Child -> Parent */
	IPC_TERMIOS_ACK,	/* Child -> Parent */
	IPC_ENV_SETUP,		/* Parent -> Child */
	IPC_ENV_ACK,		/* Child -> Parent */
	IPC_CHDIR,		/* Parent -> Child */
	IPC_CHDIR_ACK,		/* Child -> Parent */
	IPC_SETMASK,		/* Parent -> Child */
	IPC_SETMASK_ACK,	/* Child -> Parent */
	IPC_SIGCATCH,		/* Parent -> Child */
	IPC_SIGCATCH_ACK,	/* Child -> Parent */
	IPC_SETID,		/* Parent -> Child */
	IPC_SETID_ACK,		/* Child -> Parent */
	IPC_SETGROUPS,		/* Parent -> Child */
	IPC_SETGROUPS_ACK,	/* Child -> Parent */
	IPC_LAST,
};

struct porch_env {
	size_t			 setsz;
	size_t			 unsetsz;
	bool			 clear;
	char			 envstr[];
};

struct porch_process {
	lua_State		*L;
	struct porch_term	*term;
	porch_ipc_t		 ipc;
	sigset_t		 sigcaughtmask;
	sigset_t		 sigmask;
	int			 cmdsock;
	pid_t			 pid;
	int			 last_signal;
	int			 status;
	int			 termctl;
	uid_t			 uid;
	gid_t			 gid;
	bool			 raw;
	bool			 released;
	bool			 eof;
	bool			 buffered;
	bool			 error;
	bool			 draining;
};

struct porch_setgroups {
	int		setgroups_cnt;
	gid_t		setgroups_gids[];
};

#define	PORCH_SETGROUPS_SIZE(cnt) \
    (sizeof(struct porch_setgroups) + ((cnt) * sizeof(gid_t)))

struct porch_setid {
	int		setid_flags;
#define	SID_SETUID	0x01
#define	SID_SETGID	0x02
	uid_t		setid_uid;
	gid_t		setid_gid;
};

struct porch_sigcatch {
	sigset_t		 mask;
	bool			 catch;
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

static inline int
porch_lua_ipc_send_acked_payload(lua_State *L, struct porch_process *proc,
    struct porch_ipc_msg **msgp, enum porch_ipc_tag ack_type, size_t *payloadsz,
    void **payload)
{
	struct porch_ipc_msg *msg;
	int error;

	assert((payloadsz != NULL) == (payload != NULL));
	msg = *msgp;
	error = porch_ipc_send(proc->ipc, msg);
	if (error != 0)
		error = errno;

	porch_ipc_msg_free(msg);
	msg = NULL;

	if (error != 0) {
		luaL_pushfail(L);
		lua_pushstring(L, strerror(error));
		return (2);
	}

	/* Wait for ack */
	if (porch_ipc_wait(proc->ipc, NULL) == -1) {
		error = errno;
		goto err;
	}

	if (porch_ipc_recv(proc->ipc, &msg) != 0) {
		error = errno;
		goto err;
	} else if (msg == NULL) {
		luaL_pushfail(L);
		lua_pushstring(L, "unknown unexpected message received");
		return (2);
	} else if (porch_ipc_msg_tag(msg) != ack_type) {
		luaL_pushfail(L);
		lua_pushfstring(L, "unexpected message type '%d'",
		    porch_ipc_msg_tag(msg));
		porch_ipc_msg_free(msg);
		return (2);
	}

	if (payloadsz == NULL) {
		/* If we don't have a payload, we can tap out now. */
		*msgp = NULL;
		porch_ipc_msg_free(msg);
	} else {
		/*
		 * Otherwise, we extract the payload details and return the
		 * message that they came from for the caller to free at their
		 * own leisure.
		 */
		*msgp = msg;
		*payload = porch_ipc_msg_payload(msg, payloadsz);
	}

	return (0);
err:
	luaL_pushfail(L);
	lua_pushstring(L, strerror(errno));
	return (2);
}

static inline int
porch_lua_ipc_send_acked(lua_State *L, struct porch_process *proc,
    struct porch_ipc_msg *msg, enum porch_ipc_tag ack_type)
{

	return (porch_lua_ipc_send_acked_payload(L, proc, &msg, ack_type, NULL,
	    NULL));
}

static inline int
porch_lua_ipc_send_acked_errno(lua_State *L, struct porch_process *proc,
    struct porch_ipc_msg *msg, enum porch_ipc_tag ack_type)
{
	int error, *errorp;
	size_t psize;

	error = porch_lua_ipc_send_acked_payload(L, proc, &msg, ack_type,
	    &psize, (void **)&errorp);
	if (error != 0)
		return (error);

	if (psize != sizeof(*errorp)) {
		luaL_pushfail(L);
		lua_pushfstring(L, "expected payload of '%zu' bytes, got '%zu'",
		    sizeof(*errorp), psize);
		porch_ipc_msg_free(msg);
		return (2);
	}

	error = *errorp;
	porch_ipc_msg_free(msg);

	if (error != 0) {
		errno = error;
		return (-1);
	}

	return (0);
}
