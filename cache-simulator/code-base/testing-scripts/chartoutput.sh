#!/usr/bin/env bash
set -euo pipefail

make
~/microprocessor-architecture/cache-simulator/code-base/bin/testcase6 > ~/microprocessor-architecture/cache-simulator/code-base/outputs/myTraceResults
