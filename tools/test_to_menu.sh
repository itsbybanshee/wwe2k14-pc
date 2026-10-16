#!/bin/sh
# Test helper: starts the game windowed with scripted input and drives it to the main menu (about 90 s).
# Prints the log file path.
cd "$(dirname "$0")/.."
log=$(powershell -NoProfile -Command "\$env:WWE2K14_TEST_INPUT='1'; .\tools\test_launch.ps1 $*" | tr -d '\r')
sleep 50; printf 'START\n' > logs/test_input.txt
sleep 8; printf 'A\n' > logs/test_input.txt
sleep 30
echo "$log"
