#!/bin/sh
#
# Copyright (c) 2026 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Wait for a line of input, then close our side of the pty well before we
# crash.  The script will observe EOF long before we can be reaped, so it needs
# to wait for us to actually exit to notice that we were killed.
read line

exec >/dev/null 2>&1 </dev/null
sleep 1

kill -SEGV $$
