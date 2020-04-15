#!/usr/bin/env bash

make
./bin/testcase4 > my_output
diff ./testcases/testcase4.out my_output > differences.txt
