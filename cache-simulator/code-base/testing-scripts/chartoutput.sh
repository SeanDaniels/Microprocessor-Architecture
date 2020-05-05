#!/usr/bin/env bash
set -euo pipefail

make
../bin/testcase6 > ~/microprocessor-architecture/cache-simulator/code-base/outputs/myTraceResults
