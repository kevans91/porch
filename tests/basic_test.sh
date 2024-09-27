#!/bin/sh

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

1>&2 echo "Using binary: $porchbin"

fails=0
testid=1

if [ $# -ge 1 ]; then
	tests=""

	for test in "$@"; do
		tests="$tests $scriptdir/$test.orch"
	done

	set -- $tests
else
	set -- "$scriptdir"/*.orch
fi

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

for f in "$@" ;do
	f=$(basename "$f" .orch)
	testf="$scriptdir/$f.orch"
	expected_rc=0
	spawn="cat"

	case "$f" in
	timeout_*)
		expected_rc=1
		expected_timeout=$(grep '^-- TIMEOUT:' "$testf" | grep -Eo '[0-9]+')
		if [ "$expected_timeout" -le 0 ]; then
			not_ok "invalid timeout $expected_timeout"
			continue
		fi
		;;
	spawn_*|resize_*)
		spawn=""
		;;
	esac

	start=$(date +"%s")
	if [ -x "$testf" ]; then
		env PATH="$porchdir":"$PATH" "$testf"
	else
		"$porchbin" -f "$testf" -- $spawn
	fi
	rc="$?"
	end=$(date +"%s")

	if [ "$rc" -ne "$expected_rc" ]; then
		not_ok "expected $expected_rc, exited with $rc"
		continue
	fi

	case "$f" in
	timeout_*)
		;;
	*)
		ok
		continue
		;;
	esac

	elapsed=$((end - start))
	if [ "$elapsed" -lt "$expected_timeout" ]; then
		not_ok "expected $expected_timeout seconds, finished in $elapsed"
		continue
	fi

	# Also make sure it wasn't excessively long... this could be flakey.
	excessive_timeout=$((expected_timeout + 3))
	if [ "$elapsed" -ge "$excessive_timeout" ]; then
		not_ok "expected $expected_timeout seconds, finished excessively in $elapsed"
		continue
	fi

	ok
done

exit "$fails"
