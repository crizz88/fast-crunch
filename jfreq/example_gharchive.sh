#!/bin/bash
DECOMP="zcat -" # ["zcat -", "bzcat","lbzip2 -dc","unzip","gunzip",...]
FLOGIN="logins.out"
FTYPE="types.out"
ERRORLOG="error.log"

FALLBACK_URL="https://data.gharchive.org/2026-03-30-0.json.gz"
URL=""
MAX_HOURS_BACK=10

for offset in $(seq 0 $MAX_HOURS_BACK); do
    test_url="https://data.gharchive.org/$(date -u -d "${offset} hours ago" +%Y-%m-%d-%H).json.gz"
    if wget --spider -q "$test_url" 2>/dev/null; then
        URL="$test_url"
        break
    fi
done

if [ -z "$URL" ]; then
    URL="$FALLBACK_URL"
fi


echo "Streaming file from $URL"

wget --show-progress -qO- $URL | $DECOMP \
	| tee >(./jfreq -s -l 'type' 2>>$ERRORLOG | sort -t ":" -k2 -rn | grep ":" > $FTYPE) \
	| ./jfreq -s -l 'login' -n 60000 2>>$ERRORLOG | sort -t ":" -k2 -rn 2>/dev/null | grep ":" \
	| awk -F':' ' $2 > 9 { print $0 } $2 < 10 { count++ } END { if (count > 0) print "... and " count " with less than 10 entries" }' > $FLOGIN

echo "=================================="
echo "[Output]: Types file '$FTYPE'"
echo "=================================="
cat $FTYPE
echo "=================================="
echo "[Output]: Logins file '$FLOGIN'"
echo "=================================="
head -20 $FLOGIN
echo "[...]"
echo "=================================="
if [ -s "$ERRORLOG" ]; then
    echo "=================================="
    echo "Warnings/Errors occurred:"
    echo "=================================="
    head -10 "$ERRORLOG"
    echo "Ensure compiling with OBJECTTERMINATOR=\"'\n'\""
    echo "=================================="
fi
rm $ERRORLOG
