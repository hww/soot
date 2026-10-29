#!/usr/bin/env bash
# Add the build's Debug/bin to PATH via symlinks in ~/bin.
# Idempotent: safe to run on every shell start.

set -euo pipefail

BIN_DIR="${1:-$HOME/projects/hww/_c/soot.git/build/Debug/bin}"
LINK_DIR="$HOME/bin"

if [[ ! -d "$BIN_DIR" ]]; then
    echo "sootc-env: '$BIN_DIR' does not exist" >&2
    exit 1
fi

mkdir -p "$LINK_DIR"

# For every *.exe in BIN_DIR, create LINK_DIR/<stem> -> BIN_DIR/<file>.
# <stem> is the name without .exe. Skip *.pdb and anything else.
for f in "$BIN_DIR"/*.exe; do
    [[ -e "$f" ]] || continue
    base="$(basename "$f")"
    stem="${base%.exe}"
    ln -sf "$f" "$LINK_DIR/$stem"
done

# Make sure ~/bin is in PATH for this shell. Append to ~/.bashrc once.
if [[ ":$PATH:" != *":$LINK_DIR:"* ]]; then
    export PATH="$LINK_DIR:$PATH"
fi

