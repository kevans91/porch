#!/bin/sh

loop=1

trap 'echo resized; caught=$((caught + 1))' WINCH
trap 'loop=0' INT

# Release the hounds now that the signal handler is setup.
echo "ready"

caught=0

while [ "$loop" -ne 0 ]; do
	sleep 1
done

if [ "$caught" -eq 0 ]; then
	1>&2 echo "Did not observe SIGWINCH"
	exit 1
fi

echo "$caught"
