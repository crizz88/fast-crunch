# ucount

High-performance cardinality and frequency analysis for large-scale `uint32_t` datasets.

## Task
The program enables a cardinality and frequency analysis for massive numbers of uint32 integers, specifically focusing on unique counts and identifying singleton elements seen only once.

## Design and Implementation
The core is a multi-threaded producer-consumer architecture. **Readers** (producers) fill a buffer with data buckets from persistent storage via `O_DIRECT`, while **Counters** (consumers) perform the analysis.

To calculate the required metrics, the system tracks:
1. **Uniqueness:** Has the key been read before?
2. **Exclusivity:** Is the key a duplicate or seen only once?
3. **Total Count:** The absolute number of processed integers.

### Scalable Approaches
An initial attempt using a concurrent hash map with markers for occupied slots and duplicates was implemented. However, this approach was inefficient (~36 sec) due to overflow buckets, context switches, and a memory footprint exceeding 4GB. This method is only suitable for smaller datasets or specific Zipfian distributions.

### The Bitset Approach
Since the domain of `uint32_t` is 2^32, a bitset representing this domain requires exactly 512MB. By using a second bitset to indicate duplicates, the total memory requirement is fixed at 1GB. 


####**Key Optimizations:**
- **Co-alignment:** Both bitsets are co-aligned within cache lines.
- **Bitwise Logic:** Locations are calculated using fast bit operations.
- **Software Prefetching:** Manual prefetching hides RAM latency.
- **Performance:** Achieves < 14 seconds (~77M keys/sec), becoming CPU-bound rather than I/O-bound.

One shared bitset with optimistic concurrency control and relaxed atomics proved to be the most efficient setup. Given 1 billion random integers, cache contention remains low.

## Test Environment
- **System:** Asus G752VM
- **CPU:** Intel(R) Core(TM) i7-6700HQ CPU @ 2.60GHz
- **RAM:** Samsung SODIMM DDR4 Synchron 2400 MHz (0,4 ns)
- **DISK:** NVMe disk Samsung (256GB)
- **OS:** Ubuntu 25.10 Linux 6.17.0-14-generic

## Getting Started

### Generate Test Data (optional)
```bash
./generateData.sh
```

### Compile
```bash
make -j4
```

### Run Program
```bash
./ucount -uoav -f uint32_4g.bin
```

### Parameters
- `a` --> print total numbers count
- `u` --> print unique numbers count
- `o` --> print singleton count (seen once)
- `v` --> verbose output
- `d` --> run demo mode with example data output
- `f {file}` --> path of integer binary source file


## Output & Performance Metrics
```text
$ time ./ucount -uoav -f ~/uint32_4g.bin 
2 READERS and 6 COUNTERS started.
READERS finished work after 13.463623 seconds.
COUNTERS finished work after 13.673199 seconds.
Final counting took 0.103394 seconds.

949417254 unique numbers
835135937 numbers seen only once
1073741824 total numbers

Total Duration 13.776643 seconds.

real    0m13,951s
user    1m34,835s
sys     0m2,392s
```

## Demo Example
```text
===========================================
0x100 0x100 0xfff 0xfff -> 2 unique numbers
0x100 0x100 0xfff 0xfff -> 0 numbers seen only once
0x100 0x100 0xfff 0xfff -> 4 total numbers
===========================================
0x100 0x100 0x100 0x100 -> 1 unique numbers
0x100 0x100 0x100 0x100 -> 0 numbers seen only once
0x100 0x100 0x100 0x100 -> 4 total numbers
===========================================
0x100 0x100 0x800 0xfff -> 3 unique numbers
0x100 0x100 0x800 0xfff -> 2 numbers seen only once
0x100 0x100 0x800 0xfff -> 4 total numbers
===========================================
```
