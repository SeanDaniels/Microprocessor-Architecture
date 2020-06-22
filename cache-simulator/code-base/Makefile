CC = g++
OPT = -g
WARN = -Wall
CFLAGS = $(OPT) $(WARN)

# List corresponding compiled object files here (.o files)
SIM_OBJ = cache.o sim_pipe.o

TESTCASES = testcase_multmat testcase_vectoradd

#################################

# default rule
all:	$(TESTCASES)

# generic rule for converting any .cc file to any .o file
.cc.o:
	$(CC) $(CFLAGS) -c ./src/*.cc

#rule for creating the object files for all the testcases in the "testcases" folder
testcase:
	$(MAKE) -C testcases


testcase_multmat: .cc.o testcase
	$(CC) -o bin/testcase_multmat $(CFLAGS) $(SIM_OBJ) testcases/testcase_multmat.o

testcase_vectoradd: .cc.o testcase
	$(CC) -o bin/testcase_vectoradd $(CFLAGS) $(SIM_OBJ) testcases/testcase_vectoradd.o
# type "make clean" to remove all .o files plus the sim binary
clean:
	rm -f testcases/*.o
	rm -f *.o
	rm -f bin/*
	rm -f outputs/*
