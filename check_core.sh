#!/bin/bash
COREPATH=/var/lib/apport/coredump/

EXE_NAME=$1
if [ -z "$EXE_NAME" ]; then
	echo "Usage: $0 <exe_name>"
	exit 1
fi

if [[ "$EXE_NAME" != *"palan"* ]]; then
	EXE_NAME="palan-$EXE_NAME"
fi

LATEST_CORE=$(ls -t $COREPATH*$EXE_NAME* | head -n 1)

if [ -z "$LATEST_CORE" ]; then
	echo "No core file found for $EXE_NAME"
	exit 1
fi

gdb -c $LATEST_CORE build/bin/$EXE_NAME
