/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <sys/types.h>

#include <stdbool.h>

#include <lua.h>

enum orch_ipc_tag {
	IPC_RELEASE = 1,	/* Bidrectional */
	IPC_ERROR,		/* Child -> Parent */
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

struct orch_process {
	lua_State		*L;
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
