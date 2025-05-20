#!/bin/sh

signals=0
trap 'echo; echo "Interrupt caught"; signals=$((signals + 1))' INT

while [ "$signals" -lt 3 ]; do
	printf ">> "
	read line
	echo "$line"
done

# Exit with some arbitrary status so that we can test the :status() method of
# our pstatus objects.
exit 37
