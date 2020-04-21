#include "sim_pipe.h"
#include <iostream>
#include <stdlib.h>

#define KB 1024

#define N 1024 //array size

//#define DEBUG

using namespace std;

/* Test case for pipelined simuator */
/* DO NOT MODIFY */

int main(int argc, char **argv){

	cout << "VECTOR ADDITION: " << N << " elements per array" << endl << endl;

	unsigned i, j;

	// instantiates the simulator with a 1MB data memory
	sim_pipe *mips = new sim_pipe(1024*1024);

        // instantiate the cache
        cache *mycache = new cache(16*KB,                //size
                                  1,                    //associativity
                                  16,                   //cache line size
                                  WRITE_BACK,           //write hit policy
                                  WRITE_ALLOCATE,       //write miss policy
                                  5,                    //hit time
                                  100,                  //miss penalty
                                  32                    //address width
                                  );

	mycache->print_configuration();

        mips->set_cache(mycache);

	//loads program in instruction memory at address 0x10000000
	mips->load_program("asm/vector_add.asm", 0x10000000);

	// initialize registers
        // Note: arrays A, B and C are laid out contiguously in memory starting from address 0xA0000
	mips->set_gp_register(1, N); 		//number of elements in array	
	mips->set_gp_register(2, 0xA0000); 	//address of array A
	mips->set_gp_register(3, 0xA0000+N*4); 	//address of array B
	mips->set_gp_register(4, 0xA0000+N*8); 	//address of array C

	//initialize data memory and prints its content (for the specified address ranges)
	for (i = 0xA0000, j=0; j<N; i+=4, j++) mips->write_memory(i,j);
	for (i = 0xA0000+N*4, j=0; j<N; i+=4, j++) mips->write_memory(i,j*2);
	
	cout << "\nBEFORE PROGRAM EXECUTION..." << endl;
	cout << "======================================================================" << endl << endl;
	
	//prints the value of the memory and registers
	mips->print_registers();
#ifdef DEBUG
	mips->print_memory(0xA0000, 	0xA0000+4*N);
	mips->print_memory(0xA0000+4*N, 0xA0000+8*N);
	mips->print_memory(0xA0000+8*N, 0xA0000+12*N);
#endif

	// executes the program	
	cout << "\n*****************************" << endl;
	cout << "STARTING THE PROGRAM..." << endl;
	cout << "*****************************" << endl << endl;

#ifdef DEBUG
	// first 20 clock cycles
	cout << "First 20 clock cycles: inspecting the registers at each clock cycle..." << endl;
	cout << "======================================================================" << endl << endl;

	for (i=0; i<20; i++){
		cout << "CLOCK CYCLE #" << dec << i << endl;
		mips->run(1);
		mips->print_registers();
		cout << endl;
	}
#endif

	// runs program to completion
	cout << "EXECUTING PROGRAM TO COMPLETION..." << endl << endl;
	mips->run(); 

	cout << "PROGRAM TERMINATED\n";
	cout << "===================" << endl << endl;

	//prints the value of registers and data memory
	mips->print_registers();

#ifdef DEBUG
	mips->print_memory(0xA0000, 	0xA0000+4*N);
	mips->print_memory(0xA0000+4*N, 0xA0000+8*N);
	mips->print_memory(0xA0000+8*N, 0xA0000+12*N);

	mycache->print_tag_array();
#endif

	cout << endl;

	//validation
	for (i=0; i<N; i++){
		if (mips->get_memory(0xA0000+N*8+i*4)!=mips->get_memory(0xA0000+i*4)+mips->get_memory(0xA0000+N*4+i*4)){
			cout << "VALIDATION FAILED!" << endl;
			break;
		}
	}
	if (i==N) cout << "VALIDATION SUCCESSFUL!" << endl;
	
	cout << endl;

	// prints the number of instructions executed and IPC
	cout << "Instruction executed = " << dec << mips->get_instructions_executed() << endl;
	cout << "Clock cycles = " << dec << mips->get_clock_cycles() << endl;
	cout << "Stall inserted = " << dec  << mips->get_stalls() << endl;
	cout << "IPC = " << dec << mips->get_IPC() << endl;
	cout << endl;
	mycache->print_statistics();
}
