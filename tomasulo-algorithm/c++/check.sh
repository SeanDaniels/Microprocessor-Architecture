#!/usr/bin/env bash

make
./bin/testcase1 > my_output
echo "Case 1: Expected | My Version" > differences.txt
diff ./testcases/testcase1.out my_output >> differences.txt
./bin/testcase2 > my_output
echo "Case 2: Expected | My Version" >> differences.txt
diff ./testcases/testcase2.out my_output >> differences.txt
./bin/testcase3 > my_output
echo "Case 3: Expected | My Version" >> differences.txt
diff ./testcases/testcase3.out my_output >> differences.txt
./bin/testcase4 > my_output
echo "Case 4: Expected | My Version" >> differences.txt
diff ./testcases/testcase4.out my_output >> differences.txt
./bin/testcase5 > my_output
echo "Case 5: Expected | My Version" >> differences.txt
diff ./testcases/testcase5.out my_output >> differences.txt
./bin/testcase6 > my_output
echo "Case 6: Expected | My Version" >> differences.txt
diff ./testcases/testcase6.out my_output >> differences.txt
./bin/testcase7 > my_output
echo "Case 7: Expected | My Version" >> differences.txt
diff ./testcases/testcase7.out my_output >> differences.txt
./bin/testcase8 > my_output
echo "Case 8: Expected | My Version" >> differences.txt
diff ./testcases/testcase8.out my_output >> differences.txt
