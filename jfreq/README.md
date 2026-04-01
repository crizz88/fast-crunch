# jfreq

High-throughput string frequency analysis of a potentially infinite JSON stream with constant performance metrics based on input data size and available processing & I/O-ressources.

## Design and Implementation

The program uses a multi-threaded worker framework. Since the data is delivered via a pipe (decompressor), the solution handles unaligned stream fragments and short-reads without significant overhead by zero-copy intention. 

### Scalable Approach
A full JSON parser would be overkill, since there is intended a flat object stream without hierarchy. Hence, the stream is directly scanned for 'attribute_name_label:' by `memmem()` and strings and objects are terminated by `"` and `\n` or `}` (configurable at compile time).

### Storage Architecture
The absolute number of different attribute values is unknown, so a dynamic storage structure is required. A hash map allows fast seeks and updates dynamically. Variable length keys, however, should be avoided inside the bucket in order to enable performant comparisons and concurrency. Hence an external storage area is implemented. The result set is typically small, hence only a few buckets are created with the maximum size of several cachelines plus small external storage, but can be scaled up to GB of data. If an attribute already exists, a co-located counter value is incremented atomically.

### Bottleneck Management
The assumed bottlenecks are network transfer and/or decompression pipe. Hence, only one stream reader and one parser thread are used to leave processing power to the preceding pipe. `zcat` or `lbzip2` enable fast parallel decompression. Local JSON-files can be processed with massive parallelism (compile time) and specified with the '-f' flag. A subsequent sort is performed in the pipe.

## Future Work
- Path-aware scanning (e.g., `-l actor.login`)
- Support for nested object depth filtering

## Compile
`make -j4`

## Usage

### Stream Processing (hint: compile with single reader for stream)
`./jfreq -s -l 'attribute' < {stream}`

### Local File Processing (hint: compile with multiple readers for SSD, single reader for HDD)
`./jfreq -f 'path' -l 'attribute'`

### Parameters
- `-v` --> verbose
- `-l {label}` --> attribute name label
- `-n {number}` --> expected different values of attribute
- `-f {file_name}` --> path of local uncompressed file copy
- `-s` --> json stream

## Example Output (compare example_gharchive.sh)
```text
PushEvent: 94723
User: 21571
PullRequestEvent: 17951
Bot: 17656
CreateEvent: 12054
IssuesEvent: 6815
IssueCommentEvent: 6211
DeleteEvent: 4616
WatchEvent: 4186
Organization: 3729
PullRequestReviewEvent: 2271
PullRequestReviewCommentEvent: 2269
ReleaseEvent: 1061
ForkEvent: 818
MemberEvent: 251
GollumEvent: 92
PublicEvent: 65
CommitCommentEvent: 56
DiscussionEvent: 33
```
## Test Environment
- **System:** Asus G752VM
- **CPU:** Intel(R) Core(TM) i7-6700HQ CPU @ 2.60GHz
- **RAM:** Samsung SODIMM DDR4 Synchron 2400 MHz (0,4 ns)
- **DISK:** NVMe disk Samsung (256GB)
- **OS:** Ubuntu 25.10 Linux 6.17.0-14-generic

## Benchmarks

### Performance jfreq vs. jq from decompressor pipe (>6x faster)
- **jfreq:** `time cat json.gz | zcat - | ./jfreq -s -l 'login' -n 60000 > /dev/null`  
  `real 0m1,725s | user 0m1,851s | sys 0m0,240s`
- **jq:** `time cat json.gz | zcat - | jq -n 'reduce (inputs | .. | objects | .login? | strings) as $t ({}; .[$t] += 1)' > /dev/null`  
  `real 0m11,498s | user 0m13,069s | sys 0m0,239s`

### Performance jfreq vs. jq multi-threaded local uncompressed file on SSD (>100x faster)
- **jfreq:** `time ./jfreq -f ~/gh.json -l 'login' -n 60000 > /dev/null`  
  `real 0m0,102s | user 0m0,235s | sys 0m0,114s`
- **jq:** `time jq -n 'reduce (inputs | .. | objects | .login? | strings) as $t ({}; .[$t] += 1)' ~/gh.json > /dev/null`  
  `real 0m10,729s | user 0m10,654s | sys 0m0,074s`

### Benchmark: The ">20GB Single-Line JSON" Industry Challenge
Processing a massive ~21GB single-line JSON file containing 400M records.
- **Python / jq:** Crash (OOM) or hang due to DOM-tree overhead.
- **jfreq:** ~ 13.4 seconds (~1.58 GB/s throughput on SSD with 2 reader and 6 counter threads, 4MB blocks).
- **Memory Footprint:** Constant (< 50MB), regardless of file or line size.

**Compile flags for this benchmark:**  
`-DN_COUNTERS=6 -DN_READERS=2 -DOBJECTTERMINATOR="'}'" -DPOOL_SIZE=12 -DFIO_SIZE=4194304 -DHASH_MAP_BUCKET_SLOT=4 -DEXTERNAL_STORAGE_SIZE=512`

```bash
$ time ./jfreq -f ~/bigf.json -l 'model' > /dev/null
real	0m13,385s
user	1m9,116s
sys	0m8,360s
```

*Created by **cR!zZ** - Specialized in Storage Engines & High-Performance Data Processing.*
