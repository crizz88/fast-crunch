#!/bin/bash
DECOMP="bzcat -" # ["bzcat","lbzip2 -dc",...]

URL="https://quiz.storpool.com/bigf.json.bz2"
DECOMPRESSED="./bigf.json"
echo "Decompressing $DECOMPRESSED from $URL"

# Streaming from server
wget --show-progress -qO- $URL | $DECOMP > $DECOMPRESSED


