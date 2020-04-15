#!/usr/bin/env bash

make
./bin/testcase2 > my_output
diff ./testcases/testcase2.out my_output > differences.txt
