#!/usr/bin/env bash
set -euo pipefail

make clean
touch ~/microprocessor-architecture/cache-simulator/code-base/outputs/vector-mult-output.txt
make 
./bin/testcase_multmat > ./outputs/vector-mult-output.txt




