#!/bin/bash
make clean
set -e
make -j8
pv upgrade_np1380.bin | ncat np1380 12345 --send-only
make clean
