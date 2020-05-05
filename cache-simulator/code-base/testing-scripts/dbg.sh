#!/usr/bin/env bash
set -euo pipefail

make
./bin/testcase0 > myoutput
diff myoutput testcases/testcase0.out > diffs.txt
./bin/testcase1 > myoutput
diff myoutput testcases/testcase1.out > diffs.txt
./bin/testcase2 > myoutput
diff myoutput testcases/testcase2.out > diffs.txt
./bin/testcase3 > myoutput
diff myoutput testcases/testcase3.out > diffs.txt
./bin/testcase4 > myoutput
diff myoutput testcases/testcase4.out > diffs.txt
./bin/testcase5 > myoutput
diff myoutput testcases/testcase5.out > diffs.txt
