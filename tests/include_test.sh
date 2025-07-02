#!/bin/sh
#
# Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

scriptdir=$(dirname $(realpath "$0"))
if [ -n "$PORCHBIN" ]; then
	porchbin="$PORCHBIN"
else
	porchbin="$scriptdir/../src/porch"
	if [ ! -x "$porchbin" ]; then
		porchbin="$(which porch)"
	fi
fi
if [ ! -x "$porchbin" ]; then
	1>&2 echo "Failed to find a usable porch binary"
	exit 1
fi

porchdir="$(dirname "$porchbin")"

if [ -n "$PORCHLUA_PATH" ]; then
	cd "$PORCHLUA_PATH"
fi

# For convenience, to guarantee that we have an env var to search for
export PORCHTESTS=yes

1>&2 echo "=========="
1>&2 echo "Using binary: $porchbin"
1>&2 echo "++ ENVIRONMENT"
1>&2 env
1>&2 echo "++"
1>&2 echo "=========="

fails=0
testid=1

echo "1..3"

ok()
{
	local f="$1"

	echo "ok $testid - $f"
	testid=$((testid + 1))
}

not_ok()
{
	local f="$1"

	fails=$((fails + 1))
	echo "not ok $testid - $f: see output above"
	testid=$((testid + 1))
}

# Check: basic -i directive
if $porchbin -f "$scriptdir"/include_basic.orch -i "$scriptdir"/include_globals.lua; then
	ok "include_basic"
else
	not_ok "include_basic"
fi

# Check: multiple -i directive
if $porchbin -f "$scriptdir"/include_multi.orch \
    -i "$scriptdir"/include_globals.lua -i "$scriptdir"/include_overrides.lua; then
	ok "include_multi"
else
	not_ok "include_multi"
fi

# Check: multiple -i directive (reversed)
if $porchbin -f "$scriptdir"/include_multi_reverse.orch \
    -i "$scriptdir"/include_overrides.lua -i "$scriptdir"/include_globals.lua; then
	ok "include_multi_reverse"
else
	not_ok "include_multi_reverse"
fi

exit "$fails"
