#!/bin/sh
# Test helper: starts the game windowed with scripted input and clicks through to Options > Gameplay > Visual
# Presentation (about 2 minutes). Prints the log file path.
cd "$(dirname "$0")/.."
log=$(powershell -NoProfile -Command "\$env:WWE2K14_TEST_INPUT='1'; .\tools\test_launch.ps1 $*" | tr -d '\r')
sleep 42; printf 'CLICK 640 300\nWAIT 800\nMOUSE OFF\n' > logs/test_input.txt
sleep 6; grep -q "live=true" "$log" || printf 'START\n' > logs/test_input.txt  # the first press is sometimes missed
until grep -q "live=true" "$log"; do sleep 2; done; sleep 3
printf 'MOUSE OFF
DOWN
DOWN
DOWN
DOWN
DOWN
WAIT 800
A
WAIT 2000
DOWN
DOWN
WAIT 800
A
WAIT 2000
DOWN
DOWN
WAIT 800
A
WAIT 2500
' > logs/test_input.txt
sleep 14
echo "$log"
