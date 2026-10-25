#!/usr/bin/env bash
# Stops the game as soon as it starts and attaches gdb running a batch command file.
#   gdbtrace.sh <process name> <gdb command file> <output file>
set -euo pipefail
[ $# -eq 3 ] || { echo "usage: gdbtrace.sh <process name> <gdb command file> <output file>" >&2; exit 1; }
comm=${1:0:15} cmds=$2 out=$3
here=$(cd "$(dirname "$0")" && pwd)

pid=""
deadline=$((SECONDS + 120))
while [ -z "$pid" ]; do
    [ $SECONDS -lt $deadline ] || { echo "gdbtrace: $comm did not start" >&2; exit 1; }
    pid=$(pgrep -n -x "$comm" || true)
done
kill -STOP "$pid"
echo "gdbtrace: stopped $comm ($pid), attaching" >&2

gdb -q -batch -p "$pid" -iex "set debuginfod enabled off" \
    -ex "set pagination off" \
    -ex "set breakpoint pending on" \
    -ex "handle SIGSYS SIGXFSZ SIGUSR1 SIGUSR2 SIGPWR SIGXCPU SIG34 SIG35 SIG36 SIGSTOP SIGCONT nostop noprint pass" \
    -x "$cmds" \
    -ex "python exec(open('$here/gdbtrace-continue.py').read())" > "$out" 2>&1
