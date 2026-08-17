#!/bin/bash
./build/burptui &
PID=$!
sleep 1
# Send Tab 3 times using xdotool if possible, or just write to stdin?
# Writing to stdin of a background process for TUI is hard.
