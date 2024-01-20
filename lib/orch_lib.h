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
	IPC_RELEASE = 1,
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
	int			 cmdsock;
	pid_t			 pid;
	int			 status;
	int			 termctl;
	bool			 raw;
	bool			 released;
	bool			 eof;
	bool			 buffered;
};

/* orch_ipc.c */
int orch_ipc_close(void);
void orch_ipc_open(int);
bool orch_ipc_okay(void);
int orch_ipc_recv(struct orch_ipc_msg **);
int orch_ipc_send(struct orch_ipc_msg *);
int orch_ipc_wait(void);

/* orch_spawn.c */
int orch_release(void);
int orch_spawn(int, const char *[], struct orch_process *);
