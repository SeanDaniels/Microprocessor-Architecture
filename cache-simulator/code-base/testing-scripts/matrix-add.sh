#!/usr/bin/env bash
set -euo pipefail

make clean
touch ~/microprocessor-architecture/cache-simulator/code-base/outputs/vector-add-output.txt
make 
./bin/testcase_vectoradd > ./outputs/vector-add-output.txt
