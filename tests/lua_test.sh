#!/bin/sh
#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

scriptdir=$(dirname $(realpath "$0"))
schemes="lua${LUA_VERSION_MAJOR}${LUA_VERSION_MINOR}"
schemes="$schemes lua${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"

for bin in $schemes; do
	if command -v $bin >/dev/null; then
		LUA="$bin"
		break
	fi
done

if [ -z "$LUA" ]; then
	1>&2 "Could not find suitable binary named any of: $schemes"
	exit 1
fi

if [ $# -ge 1 ]; then
	tests=""

	for test in "$@"; do
		tests="$tests $scriptdir/$test.lua"
	done

	set -- $tests
else
	set -- "$scriptdir"/test_*.lua
fi

fails=0
testid=1
echo "1..$#"

ok()
{

	echo "ok $testid - $f"
	testid=$((testid + 1))
}

not_ok()
{
	msg="$1"

	echo "not ok $testid - $f: $msg"
	testid=$((testid + 1))
	fails=$((fails + 1))
}

for f in "$@"; do
	f=$(basename "$f" ".lua")
	command $LUA "$scriptdir"/"$f".lua
	rc=$?

	if [ "$rc" -eq 0 ]; then
		ok
	else
		not_ok "expected 0, exited with $rc"
	fi
done
