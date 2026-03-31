#!/bin/bash

git clone https://github.com/Tinram/RND64.git
rm ./uint32_4g.bin
RND64/bin/rnd64 -a 4g ./uint32_4g.bin
rm -rf RND64
