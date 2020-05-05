#!/usr/bin/env bash
set -euo pipefail

make
./bin/testcase6 > myTraceResults
