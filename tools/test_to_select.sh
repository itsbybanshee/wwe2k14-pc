#!/bin/sh
# Test helper: main menu (test_to_menu.sh), then Play > One on One > Normal with mouse clicks, then A to claim 1P
# on the wrestler select screen (about 2 minutes). Prints the log file path.
cd "$(dirname "$0")/.."
log=$(./tools/test_to_menu.sh "$@" | tail -1)
printf 'CLICK 612 414\nWAIT 2500\nCLICK 299 414\nWAIT 1500\nMOUSE OFF\n' > logs/test_input.txt
sleep 18; printf 'A\nWAIT 1500\n' > logs/test_input.txt; sleep 4
echo "$log"
