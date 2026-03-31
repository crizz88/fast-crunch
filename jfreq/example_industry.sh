#!/bin/bash
DECOMP="bzcat -" # ["bzcat","lbzip2 -dc",...]

ERRORLOG="error.log"
URL="https://quiz.storpool.com/bigf.json.bz2"
echo "Streaming file from $URL"

# Compile with OBJECTTERMINATOR="'}'" for this json
#export OBJECTTERMINATOR="\"'}'\""
#make clean && make -j4
# Streaming from server
wget --show-progress -qO- $URL | $DECOMP | ./jfreq -s -l 'model' 2>>$ERRORLOG | sort -t ':' -k2 -rn


if [ -s "$ERRORLOG" ]; then
    echo "=================================="
    echo "Warnings/Errors occurred:"
    echo "=================================="
    head -c 200 "$ERRORLOG"
    echo -e "[...]\nEnsure compiling with OBJECTTERMINATOR=\"'}'\""
    echo "=================================="
fi
rm $ERRORLOG

