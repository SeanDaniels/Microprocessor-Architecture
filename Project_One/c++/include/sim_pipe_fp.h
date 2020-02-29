#ifndef SIM_PIPE_FP_H_
#define SIM_PIPE_FP_H_

#include <stdio.h>
#include <list>
#include <queue>
#include "definitions.h"
#include "sp_registers.h"
#include "exec_unit.h"

using namespace std;

class sim_pipe_fp{

  //instruction memory
  instruction_t instr_memory[PROGRAM_SIZE];
  instruction_t STALL;

  //GP Registers
  unsigned int_gp_reg[NUM_GP_REGISTERS];
  unsigned fp_gp_reg[NUM_GP_REGISTERS];

  //SP registers
  sp_registers* sp_register_bank;

  //base address in the instruction memory where the program is loaded
  unsigned instr_base_address;

	//data memory - should be initialize to all 0xFF
	unsigned char *data_memory;

	//memory size in bytes
	unsigned data_memory_size;

	//memory latency in clock cycles
	unsigned data_memory_latency;

	//execution units
  unsigned max_integer_units;
  unsigned num_integer_units;
  unsigned integer_unit_latency;
  unsigned max_adder_units;
  unsigned num_adder_units;
  unsigned adder_unit_latency;
  unsigned max_mult_units;
  unsigned num_mult_units;
  unsigned mult_unit_latency;
  unsigned max_div_units;
  unsigned num_div_units;
  unsigned div_unit_latency;

  //flags
  unsigned SH_EXE_stall;
  unsigned SH_MEM_stall;
  unsigned CH_stall;
  unsigned DH_RAW_stall;
  unsigned DH_WAW_stall;
  unsigned EOP_flag;
  unsigned CH_stalls_inserted;

  //counters
  unsigned num_stalls;
  unsigned num_control_stalls;
  unsigned num_instructions;
  unsigned num_cycles;

  //data structures to handle execution process
  list<exec_unit*>execution_units;


public:

	//instantiates the simulator with a data memory of given size (in bytes) and latency (in clock cycles)
	/* Note:
     - initialize the registers to UNDEFINED value
	   - initialize the data memory to all 0xFF values
	*/
	sim_pipe_fp(unsigned data_mem_size, unsigned data_mem_latency);

	//de-allocates the simulator
	~sim_pipe_fp();

	// adds one or more execution units of a given type to the processor
  // - exec_unit: type of execution unit to be added
  // - latency: latency of the execution unit (in clock cycles)
  // - instances: number of execution units of this type to be added
  void init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances=1);

	//loads the assembly program in file "filename" in instruction memory at the specified address
	void load_program(const char *filename, unsigned base_address=0x0);

	//runs the simulator for "cycles" clock cycles (run the program to completion if cycles=0)
	void run(unsigned cycles=0);

	//resets the state of the simulator
        /* Note:
	   - registers should be reset to UNDEFINED value
	   - data memory should be reset to all 0xFF values
	*/
	void reset();

	// returns value of the specified special purpose register for a given stage (at the "entrance" of that stage)
        // if that special purpose register is not used in that stage, returns UNDEFINED
        //
        // Examples (refer to page C-37 in the 5th edition textbook, A-32 in 4th edition of textbook)::
        // - get_sp_register(PC, IF) returns the value of PC
        // - get_sp_register(NPC, ID) returns the value of IF/ID.NPC
        // - get_sp_register(NPC, EX) returns the value of ID/EX.NPC
        // - get_sp_register(ALU_OUTPUT, MEM) returns the value of EX/MEM.ALU_OUTPUT
        // - get_sp_register(ALU_OUTPUT, WB) returns the value of MEM/WB.ALU_OUTPUT
	// - get_sp_register(LMD, ID) returns UNDEFINED
	/* Note: you are allowed to use a custom format for the IR register.
           Therefore, the test cases won't check the value of IR using this method.
	   You can add an extra method to retrieve the content of IR */
	unsigned get_sp_register(sp_register_t reg, stage_t stage);

  //returns value of the specified integer general purpose register
  int get_int_register(unsigned reg);

  //set the value of the given integer general purpose register to "value"
  void set_int_register(unsigned reg, int value);

  //returns value of the specified floating point general purpose register
  float get_fp_register(unsigned reg);

  //set the value of the given floating point general purpose register to "value"
  void set_fp_register(unsigned reg, float value);

	//returns the IPC
	float get_IPC();

	//returns the number of instructions fully executed
	unsigned get_instructions_executed();

	//returns the number of clock cycles
	unsigned get_clock_cycles();

	//returns the number of stalls added by processor
	unsigned get_stalls();

	//prints the content of the data memory within the specified address range
	void print_memory(unsigned start_address, unsigned end_address);

	// writes an integer value to data memory at the specified address (use little-endian format: https://en.wikipedia.org/wiki/Endianness)
	void write_memory(unsigned address, unsigned value);

  unsigned load_memory(unsigned address);

	//prints the values of the registers
	void print_registers();



private:
  void run_IF();
  void run_ID();
  void run_EXE();
  void run_MEM();
  void run_WB();
/*diff*/
  void cycle_all_units();
/*diff*/
  unsigned get_unit_latency(exe_unit_t);

  bool check_unit_free(exe_unit_t);

  void incrementUnitCount(exe_unit_t);

  void decrementUnitCount(exe_unit_t);

  instruction_t get_IR(stage_t);

  unsigned get_condition(instruction_t, unsigned);

  void printunits();

  void check_RAW_dataHazard();

  void check_WAW_dataHazard(unsigned, exe_unit_t);

  bool get_finished_unit(pipe_stage_three&);

  void cycle_clock();

};

#endif /*SIM_PIPE_FP_H_*/
