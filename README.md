# High-Performance Data Crunching (C / SIMD / Zero-Copy)

This repository contains a collection of specialized tools designed to saturate modern hardware limits (NVMe, RAM-latency, CPU-pipelines). 

Developed as a response to inefficient "industry-standard" tools, these implementations prioritize **Mechanical Sympathy**, **Lock-free Atomics**, and **Software-controlled Prefetching**.

## The Tools

### 1. [jfreq](./jfreq) - The JSON Stream frequency counter
A zero-copy JSON scanner for massive streams and multi-gigabyte single-line files.
*   **Performance:** 22GB JSON processed in **< 14s** (~1.58 GB/s).
*   **Efficiency:** **100x faster** than `jq` or Python.
*   **Memory:** Constant footprint (< 50MB) regardless of file size.
*   *Challenge:* Handled the "Industry Challenge" where standard parsers crashed or took hours.

### 2. [ucount](./ucount) - The Billion-Integer frequency analyzer
A lock-free bitset implementation to count unique values and singletons in massive binary datasets.
*   **Throughput:** 1 Billion `uint32_t` processed in **< 14s** (~77M keys/sec).
*   **Tech:** Manual prefetching to hide RAM latency, multi-threaded worker pool.
*   **Efficiency:** Completed in seconds what Python 3 couldn't finish in **14+ hours** (stuck in Swapping).

---

## Tech Stack
- **Language:** C (C11/GNU)
- **Optimizations:** SIMD (AVX2), Software Prefetching, Lock-free Atomics, `mmap` / `O_DIRECT`.
- **License:** MIT

---
*Created by cR!zZ - Specialized in Storage Engines & High-Performance Data Processing.*
