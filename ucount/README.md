#Task:
The program enables a cardinality and frequency analysis for large numbers of uint32_t.

#Comments:

The idea is a multi-threaded solution. Readers (producer) fill a buffer with data buckets from
persistent storage via O_DIRECT and Counters (consumer) perform countings. 
(1) Uniqueness: We need to check, if the key was already read.
(2) Only once: We need to check the unique keys for duplicates and remove them from this key figure.
(3) Total numbers: We need to check the absolute numbers.
Hence we only need to remember already read keys and mark duplicates for calculations.
Scalable Approaches:
A first shot was to store the keys in a concurrent hash map with a marker for occupied slot and duplicates as
a generic general purpose. 
However, this solution was terribly slow (about 36 sec) and performance degenerations due to overflow buckets,
context switches, ..., and of course a map size of > 4GB in the end. So, this would be a good idea for smaller
datasets or zipfian distributions.
The Bitset Approach:
The domain of uint32_t is 2^32. A bit representation of this domain in a bitset requires 512MB. A second bitset
indicates the duplicates, hence the total required size is 1GB. Both bitsets can be co-aligned within a cache
line. Moreover, locations can be calculated by simple bit operations and be prefetched. Cheaper calculations, 
less RAM, more potential disk I/O. With the result of < 14 seconds (CPU-bound, ~77mio keys/sec).
Considerations:
One shared bitset with optimistic concurrency control and relaxed atomics seems to be the best option in this
setup. 1 billion random integers are not likely to collide (low cache contention). Local lock-free copies 
would increase the memory footprint, leading to cache thrashing, and require a subsequent merging. Sharding 
would require additional data movement due to the randomized input.

#Test Environment:
System
Asus (G752VM)
CPU: Intel(R) Core(TM) i7-6700HQ CPU @ 2.60GHz
RAM: Samsung SODIMM DDR4 Synchron 2400 MHz (0,4 ns)
DISK: NVMe disk Samsung (256GB)

Ubuntu 25.10 Linux 6.17.0-14-generic



#Generate Test Data (optional):
./generateData.sh

#Compile:
make -j4

#Run Program:
./ucount -abv -f uint32_4g.bin

-u --> print unique numbers 
-o --> print only once seen 
-a --> print total numbers
-d --> count demo data from example
-v --> verbose

-f {file_name} --> path of 4g integer binary source file

#Output:
$ time ./ucount -uoav -f ~/uint32_4g.bin 
2 READERS and 6 COUNTERS started.
READERS finished work after 13.463623 seconds.
COUNTERS finished work after 13.673199 seconds.
Final counting took 0.103394 seconds.
949417254 unique numbers
835135937 numbers seen only once
1073741824 total numbers
Total Duration 13.776643 seconds.

real	0m13,951s
user	1m34,835s
sys	0m2,392s

#Demo Example
Demo mode:
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

