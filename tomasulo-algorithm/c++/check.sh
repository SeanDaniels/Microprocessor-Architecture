#!/usr/bin/env bash

make
./bin/testcase3 > my_output
diff ./testcases/testcase3.out my_output > differences.txt
