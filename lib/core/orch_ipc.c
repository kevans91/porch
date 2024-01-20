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

static struct orch_ipc_msgq *head, *tail;
static int sockfd = -1;

static int orch_ipc_drain(void);

int
orch_ipc_close(void)
{
	struct orch_ipc_msgq *msgq, *next;
	int error;

	error = 0;
	if (sockfd != -1) {
		shutdown(sockfd, SHUT_WR);

		/*
		 * orch_ipc_drain() should hit EOF then close the socket.
		 */
		while (sockfd != -1 && error == 0) {
			orch_ipc_wait();

			error = orch_ipc_drain();
		}
	}

	msgq = head;
	while (msgq != NULL) {
		next = msgq->next;

		free(msgq->msg);
		free(msgq);
		msgq = next;
	}

	tail = NULL;
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

		if (head == NULL)
			head = tail = msgq;
		else
			tail = tail->next = msgq;

		msg = NULL;
		msgq = NULL;
	}

	return (0);
eof:

	close(sockfd);
	sockfd = -1;
	return (0);
}

int
orch_ipc_recv(struct orch_ipc_msg **omsg)
{
	struct orch_ipc_msgq *msgq;

	*omsg = NULL;

	if (orch_ipc_drain() != 0)
		return (-1);

	if (head == NULL)
		return (0);

	/* Dequeue a msg */
	msgq = head;
	head = head->next;

	/* Free the container */
	*omsg = msgq->msg;

	free(msgq);
	msgq = NULL;

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

int
orch_ipc_wait(void)
{
	fd_set rfd;
	int error;

	FD_ZERO(&rfd);
	do {
		if (sockfd == -1) {
			errno = ECONNRESET;
			return (-1);
		}

		FD_SET(sockfd, &rfd);

		error = select(sockfd + 1, &rfd, NULL, NULL, NULL);
	} while (error == -1 && errno == EINTR);

	return (error);
}
