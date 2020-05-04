#!/usr/bin/env bash
set -euo pipefail

make
./bin/testcase_vectoradd > myoutput
