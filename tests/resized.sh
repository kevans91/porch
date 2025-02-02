#!/bin/sh
#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

loop=1
caught=0

trap 'echo resized; caught=$((caught + 1))' WINCH
trap 'loop=0' INT

# Release the hounds now that the signal handler is setup.
echo "ready"

while [ "$loop" -ne 0 ]; do
	sleep 1
done

if [ "$caught" -eq 0 ]; then
	1>&2 echo "Did not observe SIGWINCH"
	exit 1
fi

echo "$caught"
