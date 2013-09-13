#! /bin/sh

if [ -z "$1" ]; then
    echo "Usage: $0 pattern"
    exit 1
fi

grep -RIn --color=auto -e "$1" ./
