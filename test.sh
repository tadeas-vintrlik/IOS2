#!/bin/sh
# Simple call to make it easier
if [ "$1" = "valgrind" ]
then
    valgrind --leak-check=full --show-leak-kinds=all -s ./proj2 1 2 3 4
    rm proj.out
    exit 0
fi

if [ "$1" = "loop" ]
then
    lines=4
    for i in $(seq 1 1000)
    do
        echo "$i" > /dev/null
        ./proj2 1 2 3 4
        real="$(wc -l < proj.out)"
        [ "$real" -ne "$lines" ] && echo "Error in synchronisation expected $lines got $real" echo "does it have $lines lines?" && cat proj.out && exit 1
        rm proj.out
    done
    exit 0
fi

./proj2 5 4 100 100
echo "Contents of proj.out:"
cat proj.out
rm proj.out
