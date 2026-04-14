#!/usr/bin/env bash
# pipeline.sh — End-to-end Slugs synthesis pipeline
#
# Compiles a .structuredslugs spec to .slugsin, runs Slugs synthesis,
# and converts the resulting strategy to a Graphviz .dot file via mealy2dot.py.
# If the spec is unrealizable, a counter-strategy is extracted instead.
#
# Usage:
#   pipeline.sh <basename>
#
# Arguments:
#   basename   Base name shared by the input/output files (no extension).
#              The script expects <basename>.structuredslugs in the current
#              directory and writes <basename>.slugsin, <basename>.json,
#              <basename>.err, and <basename>.dot.
#
# Example:
#   pipeline.sh coffee_out
#
# Dependencies:
#   - python3 with StructuredSlugsParser/compiler.py on the path
#   - slugs binary (default: ../src/slugs relative to this script, or PATH)
#   - mealy2dot.py in the same directory as this script
#   - python3 graphviz package (optional, for PDF/PNG output)
#
# Environment:
#   SLUGS_BIN   Override the slugs binary path (default: ../src/slugs or PATH)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

GREEN='\033[1;32m'
ORANGE='\033[38;5;208m'
RESET='\033[0m'

# ---------------------------------------------------------------------------
# Argument check
# ---------------------------------------------------------------------------
if [ $# -ne 1 ]; then
    echo "Usage: $0 <basename>"
    echo "Example: $0 coffee_out"
    exit 1
fi

BASE="$1"
STRUCTURED="${BASE}.structuredslugs"
SLUGSIN="${BASE}.slugsin"
JSON="${BASE}.json"
ERR="${BASE}.err"

if [ ! -f "$STRUCTURED" ]; then
    echo "Error: '${STRUCTURED}' not found." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Locate slugs binary
# ---------------------------------------------------------------------------
if [ -n "${SLUGS_BIN:-}" ]; then
    SLUGS="$SLUGS_BIN"
elif command -v slugs &>/dev/null; then
    SLUGS="slugs"
elif [ -x "${SCRIPT_DIR}/../src/slugs" ]; then
    SLUGS="${SCRIPT_DIR}/../src/slugs"
else
    echo "Error: slugs binary not found. Add slugs to PATH, build it with 'cd src && make', or set SLUGS_BIN." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 1: Compile .structuredslugs → .slugsin
# ---------------------------------------------------------------------------
echo "Step 1: Compiling StructuredSlugs → slugsin"
python3 "${SCRIPT_DIR}/StructuredSlugsParser/compiler.py" "$STRUCTURED" > "$SLUGSIN"

# ---------------------------------------------------------------------------
# Step 2: Run Slugs synthesis
# ---------------------------------------------------------------------------
echo "Step 2: Running Slugs synthesis"
if "$SLUGS" "$SLUGSIN" --explicitStrategy --jsonOutput > "$JSON" 2> "$ERR"; then
    echo -e "${GREEN}Spec is realizable${RESET}"
else
    result_line=$(grep -m1 "RESULT:" "$BASE.err")
    if [[ "$result_line" == *"unrealizable"* ]]; then
        echo -e "${ORANGE}Spec is unrealizable — extracting counter-strategy${RESET}"
        "$SLUGS" "$SLUGSIN" --counterStrategy --jsonOutput > "$JSON" 2>> "$ERR"
    else
        echo "Error: slugs failed unexpectedly. Check ${ERR}." >&2
        exit 1
    fi
fi

# ---------------------------------------------------------------------------
# Step 3: Convert to Graphviz .dot
# ---------------------------------------------------------------------------
echo "Step 3: Converting strategy to DOT"
python3 "${SCRIPT_DIR}/mealy2dot.py" "$SLUGSIN" "$JSON"

echo "Pipeline completed successfully."
