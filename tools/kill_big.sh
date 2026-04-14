#!/usr/bin/env bash
# kill_big.sh — Kill a process when a watched file exceeds a size threshold
#
# Polls a file once per second and sends SIGTERM to a process when the file
# grows beyond the specified byte limit.  Useful for capping runaway Slugs
# jobs that produce very large output or strategy files.
#
# Usage:
#   kill_big.sh <PID> <FILE> <MAX_SIZE_BYTES>
#
# Arguments:
#   PID             Process ID to kill when the size limit is reached.
#   FILE            Path to the file to watch (need not exist yet at startup).
#   MAX_SIZE_BYTES  Maximum allowed file size in bytes.
#
# Example:
#   # Run slugs in background, kill it if strategy JSON exceeds 500 MB
#   src/slugs myspec.slugsin --explicitStrategy --jsonOutput > out.json &
#   tools/kill_big.sh $! out.json $((500 * 1024 * 1024))
#
# Exit codes:
#   0   Process was killed because the file exceeded the limit, or the
#       process exited on its own before the limit was reached.
#   1   Bad arguments.

set -euo pipefail

if [ $# -ne 3 ]; then
    echo "Usage: $0 <PID> <FILE> <MAX_SIZE_BYTES>" >&2
    echo "Example: $0 12345 out.json $((500 * 1024 * 1024))" >&2
    exit 1
fi

PID=$1
FILE=$2
MAX_SIZE=$3

echo "Watching '${FILE}' — will kill PID ${PID} if size exceeds ${MAX_SIZE} bytes."

while kill -0 "$PID" 2>/dev/null; do
    if [ -f "$FILE" ]; then
        SIZE=$(stat -c%s "$FILE")
        if [ "$SIZE" -ge "$MAX_SIZE" ]; then
            echo "File '${FILE}' reached ${SIZE} bytes (limit ${MAX_SIZE}). Killing PID ${PID}..."
            kill "$PID"
            exit 0
        fi
    fi
    sleep 1
done

echo "Process ${PID} exited before the size limit was reached."
exit 0
