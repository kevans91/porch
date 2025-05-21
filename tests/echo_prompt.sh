#!/bin/sh

delayed_exit() {
	local tmo=${1-5}
	local signal=${2-USR1}

	# Close everything and ignore any incidental output attempts with the
	# hope that everything still works according to plan.
	trap '' HUP PIPE
	exec <&- 1>&- 2>&-

	if [ "$tmo" -ne 0 ]; then
		sleep "$tmo"
	fi

	trap - "$signal"
	kill -"$signal" $$
}

signals=0
trap 'echo; echo "Interrupt caught"; signals=$((signals + 1))' INT
trap 'delayed_exit 3' USR1
trap 'delayed_exit 0 USR2' USR2

while [ "$signals" -lt 3 ]; do
	printf ">> "
	read line
	echo "$line"
done

# Exit with some arbitrary status so that we can test the :status() method of
# our pstatus objects.
exit 37
