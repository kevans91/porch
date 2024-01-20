/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/select.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "orch.h"
#include "orch_lib.h"

struct orch_ipc_msgq {
	struct orch_ipc_msg	*msg;
	struct orch_ipc_msgq	*next;
};

static struct orch_ipc_register {
	orch_ipc_handler	*handler;
	void			*cookie;
} orch_ipc_registration[IPC_LAST - 1];

static struct orch_ipc_msgq *head, *tail;
static int sockfd = -1;

static int orch_ipc_drain(void);
static int orch_ipc_pop(struct orch_ipc_msg **);

int
orch_ipc_close(void)
{
	int error;

	error = 0;
	if (sockfd != -1) {
		shutdown(sockfd, SHUT_WR);

		/*
		 * orch_ipc_drain() should hit EOF then close the socket.
		 */
		while (sockfd != -1 && error == 0) {
			orch_ipc_wait(NULL);

			error = orch_ipc_drain();
		}
	}

	/*
	 * We may have hit EOF at an inopportune time, just cope with it
	 * and free the queue.
	 */
	error = orch_ipc_pop(NULL);
	assert(head == NULL);
	tail = NULL;
	for (size_t i = 0; i < IPC_LAST; i++) {
		struct orch_ipc_register *reg = &orch_ipc_registration[i];

		reg->handler = NULL;
		reg->cookie = NULL;
	}

	return (error);
}

void
orch_ipc_open(int fd)
{

	assert(sockfd == -1);
	sockfd = fd;
}

bool
orch_ipc_okay(void)
{

	return (sockfd >= 0);
}

static int
orch_ipc_drain(void)
{
	struct orch_ipc_header hdr;
	struct orch_ipc_msg *msg;
	struct orch_ipc_msgq *msgq;
	ssize_t readsz;
	size_t off, resid;

	if (sockfd == -1)
		return (0);

	for (;;) {
		readsz = read(sockfd, &hdr, sizeof(hdr));
		if (readsz == -1) {
			if (errno == EAGAIN)
				break;
			return (-1);
		} else if (readsz == 0) {
			goto eof;
		}

		/*
		 * We might have an empty payload, but we should never have less
		 * than a header's worth of data.
		 */
		if (hdr.size < sizeof(hdr)) {
			errno = EINVAL;
			return (-1);
		}

		msg = malloc(hdr.size);
		if (msg == NULL)
			return (-1);

		msgq = malloc(sizeof(*msgq));
		if (msgq == NULL) {
			free(msg);
			errno = ENOMEM;
			return (-1);
		}

		msgq->msg = msg;
		msgq->next = NULL;

		msg->hdr = hdr;

		off = 0;
		resid = hdr.size - sizeof(hdr);

		while (resid != 0) {
			readsz = read(sockfd, &msg->data[off], resid);
			if (readsz == -1) {
				if (errno != EAGAIN) {
					free(msg);
					return (-1);
				}

				/* XXX Poll? */
				continue;
			} else if (readsz == 0) {
				free(msg);
				msg = NULL;

				goto eof;
			}

			off += readsz;
			resid -= readsz;
		}

		if (head == NULL) {
			head = tail = msgq;
		} else {
			tail->next = msgq;
			tail = msgq;
		}

		msg = NULL;
		msgq = NULL;
	}

	return (0);
eof:

	close(sockfd);
	sockfd = -1;
	return (0);
}

static int
orch_ipc_pop(struct orch_ipc_msg **omsg)
{
	struct orch_ipc_register *reg;
	struct orch_ipc_msgq *msgq;
	struct orch_ipc_msg *msg;
	int error;

	error = 0;
	while (head != NULL) {
		/* Dequeue a msg */
		msgq = head;
		head = head->next;

		/* Free the container */
		msg = msgq->msg;

		free(msgq);
		msgq = NULL;

		/* Do we have a handler for it? */
		reg = &orch_ipc_registration[msg->hdr.tag - 1];
		if (reg->handler != NULL) {
			int serr;

			error = (*reg->handler)(msg, reg->cookie);
			if (error != 0)
				serr = errno;

			free(msg);
			msg = NULL;

			if (error != 0) {
				errno = serr;
				error = -1;
				break;
			}

			/*
			 * Try to dequeue another one... the handler is allowed
			 * to shut down IPC, so let's be careful here.
			 */
			continue;
		}

		/*
		 * No handler, potentially tap this one out.  If we don't have
		 * an omsg, we're just draining so we'll free the msg here.
		 */
		if (omsg == NULL) {
			free(msg);
			msg = NULL;

			continue;
		}

		*omsg = msg;
		break;
	}

	return (error);
}

int
orch_ipc_recv(struct orch_ipc_msg **omsg)
{
	struct orch_ipc_msg *rcvmsg;
	int error;

	if (orch_ipc_drain() != 0)
		return (-1);

	rcvmsg = NULL;
	error = orch_ipc_pop(&rcvmsg);
	if (error == 0)
		*omsg = rcvmsg;
	return (error);
}

int
orch_ipc_register(enum orch_ipc_tag tag, orch_ipc_handler *handler,
    void *cookie)
{
	struct orch_ipc_register *reg = &orch_ipc_registration[tag - 1];

	reg->handler = handler;
	reg->cookie = cookie;
	return (0);
}

int
orch_ipc_send(struct orch_ipc_msg *msg)
{
	ssize_t writesz;
	size_t off, resid;

retry:
	if (orch_ipc_drain() != 0)
		return (-1);

	writesz = write(sockfd, &msg->hdr, sizeof(msg->hdr));
	if (writesz == -1) {
		if (errno != EAGAIN)
			return (-1);
		goto retry;
	} else if ((size_t)writesz < sizeof(msg->hdr)) {
		errno = EIO;
		return (-1);
	}

	off = 0;
	resid = msg->hdr.size - sizeof(msg->hdr);
	while (resid != 0) {
		writesz = write(sockfd, &msg->data[off], resid);
		if (writesz == -1) {
			if (errno != EAGAIN)
				return (-1);
			continue;
		}

		off += writesz;
		resid -= writesz;
	}

	return (0);
}

#include <stdio.h>

int
orch_ipc_wait(bool *eof_seen)
{
	fd_set rfd;
	int error;

	if (eof_seen != NULL)
		*eof_seen = false;

	/*
	 * If we have any messages in the queue, don't bother polling; recv
	 * will return something.
	 */
	if (head != NULL)
		return (0);

	FD_ZERO(&rfd);
	do {
		if (sockfd == -1) {
			if (eof_seen != NULL)
				*eof_seen = true;
			return (0);
		}

		FD_SET(sockfd, &rfd);

		error = select(sockfd + 1, &rfd, NULL, NULL, NULL);
	} while (error == -1 && errno == EINTR);

	return (error);
}
