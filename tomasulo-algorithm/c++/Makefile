CC = g++
OPT = -g
WARN = -Wall
#CFLAGS = $(OPT) $(WARN)
CFLAGS = $(OPT)

# List corresponding compiled object files here (.o files)
SIM_OBJ = sim_ooo.o

#TESTCASES = testcase1 testcase2 testcase3 testcase4 testcase5 testcase6 # ECE463 testcases
TESTCASES = testcase1 testcase2

#################################

# default rule
all:	$(TESTCASES)

# generic rule for converting any .cc file to any .o file
.cc.o:
	$(CC) $(CFLAGS) -c ./*.cc

#rule for creating the object files for all the testcases in the "testcases" folder
testcase:
	$(MAKE) -C testcases

# rules for making testcases
testcase1: .cc.o testcase
	$(CC) -o bin/testcase1 $(CFLAGS) $(SIM_OBJ) testcases/testcase1.o

testcase2: .cc.o testcase
	$(CC) -o bin/testcase2 $(CFLAGS) $(SIM_OBJ) testcases/testcase2.o
# type "make clean" to remove all .o files plus the sim binary
clean:
	rm -f testcases/*.o
	rm -f *.o
	rm -f bin/*
