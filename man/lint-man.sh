#!/bin/sh

if [ ! -d man ]; then
	1>&2 echo "$0: must be run from the source root"
	exit 1
fi
if ! command -v mandoc >/dev/null; then
	1>&2 echo "Skipping lint-man; mandoc not found."
	exit 0
elif ! command -v igor >/dev/null; then
	1>&2 echo "Skipping lint-man; igor not found."
	exit 0
fi

MANPATH="$PWD"/man:$(manpath)
export MANPATH

rc=0

igorf=$(mktemp)
trap 'rc=$?; rm -f "$igorf"; exit $rc' EXIT

for page in man/man*/*; do
	if ! mandoc -Tlint -Wstyle "$page"; then
		rc=1
		1>&2 echo "ERROR: $page: mandoc -Tlint"
	fi

	case "$page" in
	*.5)
		# We filter out igor's warning about SYNOPSIS for section 5 pages,
		# because it's not common for them to have a SYNOPSIS.
		igor -D "$page" | grep -v "SYNOPSIS has not been defined" > "$igorf"
		;;
	*)
		igor -D "$page" > "$igorf"
		;;
	esac
	if [ -s "$igorf" ]; then
		rc=1
		1>&2 cat "$igorf"
		1>&2 echo "ERROR: $page: igor"
	fi
done

exit $rc
