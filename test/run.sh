#!/bin/bash

B=$(readlink -f $(dirname $0))
MAIN=runmain 

runmain() {
	( cd $B/.. ; ./main )
}

map() {
	local c="$1" i r
	shift
	for i; do 
		$c $i || r=1
	done
	return $r
}

inputs() {
	ls *.in | sed 's/\.in$//'
}

checkpipe() {
	[ "${PIPESTATUS[*]}" = "$*" ]
}

map_inputs() {
	map $1 $(inputs)
}

diffout() {
	diff -u $1.out -
}

checkdiff() {
	< $1.in $MAIN | diffout $1
	checkpipe 0 0
}

checkfn() {
	< $1.in $MAIN | ./$1.fn
	checkpipe 0 0
}

checkdifftimeout() {
	< $1.in timeout 10 $MAIN | diffout $1
	checkpipe 124 0
}

checkfntimeout() {
	< $1.in timeout 10 $MAIN | ./$1.fn
	checkpipe 124 0
}

runtest() {
	local r
	cd $B/$1
	echo Running $1 ...
	. run-test.sh; r=$?
	cd - > /dev/null
	return $r
}

if [ "$*" = "" ]; then
	map runtest $(find test -maxdepth 1 -mindepth 1 -type d -printf '%f\n')
else
	map runtest "$@"
fi