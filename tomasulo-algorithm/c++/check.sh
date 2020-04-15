#!/usr/bin/env bash

<<<<<<< HEAD
make clean
make
./bin/testcase1 > my_output
=======
make
./bin/testcase3 > my_output
diff ./testcases/testcase3.out my_output > differences.txt
>>>>>>> 99f65046983a2925fdfe7fb557a2556bd05b6a3a
