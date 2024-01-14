#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))
orchdir="$scriptdir/.."
orchbin="$orchdir/orch"

# XXX Caveat
cd "$orchdir"

fails=0
testid=1
simple_tests="simple_noecho_fail simple_match simple_nonmatch_fail"
simple_tests="$simple_tests simple_match_sequence simple_match_sequence_fail"
simple_tests="$simple_tests simple_match_sequence_twice_fail"
simple_tests="$simple_tests simple_callback simple_callback_fail"
simple_tests="$simple_tests simple_fail_handler simple_raw simple_anchored"

timeout_tests="timeout_basic timeout_global timeout_test"

one_tests="one_basic one_callback_fail"

spawn_tests="spawn_multi spawn_multi_match"

tests="$simple_tests $timeout_tests $one_tests $spawn_tests"
set -- $tests
echo "1..$#"

for f in $simple_tests $one_tests $spawn_tests; do
	testf="$scriptdir/$f.orch"

	if "$orchbin" -f "$testf" -- cat; then
		res="ok"
	else
		fails=$((fails + 1))
		res="not ok"
	fi

	echo "$res $testid - $f"
	testid=$((testid + 1))
done

# Timeout tests are all expected to fail, since we're specifically testing our
# timeout behavior.
for f in $timeout_tests; do
	testf="$scriptdir/$f.orch"
	expected_timeout=$(grep '^-- TIMEOUT:' "$testf" | grep -Eo '[0-9]+')

	if [ "$expected_timeout" -le 0 ]; then
		echo "not ok $testid - $f, invalid timeout $expected_timeout"
		testid=$((testid + 1))
		continue
	fi

	start=$(date +"%s")
	"$orchbin" -f "$testf" -- cat
	rc="$?"
	end=$(date +"%s")

	if [ "$rc" -eq 0 ]; then
		echo "not ok $testid - $f, script matched"
		testid=$((testid + 1))
		continue
	fi

	elapsed=$((end - start))
	if [ "$elapsed" -lt "$expected_timeout" ]; then
		echo "not ok $testid - $f, expected $expected_timeout seconds, finished in $elapsed"
		testid=$((testid + 1))
		continue
	fi

	# Also make sure it wasn't excessively long... this could be flakey.
	excessive_timeout=$((expected_timeout + 3))
	if [ "$elapsed" -ge "$excessive_timeout" ]; then
		echo "not ok $testid - $f, expected $expected_timeout seconds, finished excessively in $elapsed"
		testid=$((testid + 1))
		continue
	fi

	echo "ok $testid - $f"
	testid=$((testid + 1))
done
exit "$fails"
