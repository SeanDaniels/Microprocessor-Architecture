#!/usr/bin/env bash

make
./bin/testcase1 > my_output1
echo "Case 1: Expected | My Version" > differences.txt
diff ./testcases/testcase1.out my_output1 >> differences.txt
./bin/testcase2 > my_output2
echo "Case 2: Expected | My Version" >> differences.txt
diff ./testcases/testcase2.out my_output2 >> focusDiff.txt
./bin/testcase3 > my_output3
echo "Case 3: Expected | My Version" >> differences.txt
diff ./testcases/testcase3.out my_output3 >> differences.txt
./bin/testcase4 > my_output4
echo "Case 4: Expected | My Version" >> differences.txt
diff ./testcases/testcase4.out my_output4 >> differences.txt
./bin/testcase5 > my_output5
echo "Case 5: Expected | My Version" >> differences.txt
diff ./testcases/testcase5.out my_output5 >> differences.txt
./bin/testcase6 > my_output6
echo "Case 6: Expected | My Version" >> differences.txt
diff ./testcases/testcase6.out my_output6 >> differences.txt
./bin/testcase7 > my_output7
echo "Case 7: Expected | My Version" >> differences.txt
diff ./testcases/testcase7.out my_output7 >> differences.txt
