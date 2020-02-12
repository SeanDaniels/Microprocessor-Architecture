#include "sim_pipe.h"
#include <iostream>
#include <stdlib.h>

using namespace std;

/* Test case for pipelined simuator */
/* DO NOT MODIFY */

sim_t mips;

int main(int argc, char **argv){

	unsigned i, j;

	// instantiates the simulator with a 1MB data memory
	sim_pipe_init(1024*1024, 4);

	//loads program in instruction memory at address 0x10000000
	load_program("asm/control_dep.asm", 0x10000000);

	//initialize general purpose registers
	for (i=0; i<7; i++) set_gp_register(i,i);

	//initialize data memory and prints its content (for the specified address ranges)
	for (i = 0x0, j=1; i<0x20; i+=4, j+=1) write_memory(i,j);
	
	cout << "\nBEFORE PROGRAM EXECUTION..." << endl;
	cout << "======================================================================" << endl << endl;
	
	//prints the value of the memory and registers
	print_registers();
	print_memory(0x0, 0x20);

	// executes the program	
	cout << "\n*****************************" << endl;
	cout << "STARTING THE PROGRAM..." << endl;
	cout << "*****************************" << endl << endl;

	// first 15 clock cycles
	cout << "First 15 clock cycles: inspecting the registers at each clock cycle..." << endl;
	cout << "======================================================================" << endl << endl;

	for (i=0; i<15; i++){
		cout << "CLOCK CYCLE #" << dec << i << endl;
		run(1);
		print_registers();
		cout << endl;
	}

	// runs program to completion
	cout << "EXECUTING PROGRAM TO COMPLETION..." << endl << endl;
	run(); 

	cout << "PROGRAM TERMINATED\n";
	cout << "===================" << endl << endl;

	//prints the value of registers and data memory
	print_registers();
	print_memory(0x0, 0x20);
	
	cout << endl;

	// prints the number of instructions executed and IPC
	cout << "Instruction executed = " << dec << get_instructions_executed() << endl;
	cout << "Clock cycles = " << dec << get_clock_cycles() << endl;
	cout << "Stall inserted = " << dec  << get_stalls() << endl;
	cout << "IPC = " << dec << get_IPC() << endl;
	
	sim_pipe_terminate();
}
