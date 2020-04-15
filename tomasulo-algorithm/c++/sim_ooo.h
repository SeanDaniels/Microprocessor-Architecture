#ifndef SIM_OO_H_
#define SIM_OO_H_

#include <cstring>
#include <map>
#include <queue>
#include <sstream>
#include <stdbool.h>
#include <stdio.h>
#include <string>
#include <stack>
using namespace std;

#define UNDEFINED 0xFFFFFFFF // constant used for initialization
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 24
#define NUM_STAGES 4
#define MAX_UNITS 10
#define PROGRAM_SIZE 50

// instructions supported
typedef enum {
  LW,
  SW,
  ADD,
  ADDI,
  SUB,
  SUBI,
  XOR,
  AND,
  MULT,
  DIV,
  BEQZ,
  BNEZ,
  BLTZ,
  BGTZ,
  BLEZ,
  BGEZ,
  JUMP,
  EOP,
  LWS,
  SWS,
  ADDS,
  SUBS,
  MULTS,
  DIVS
} opcode_t;

// reservation stations types
typedef enum { INTEGER_RS, ADD_RS, MULT_RS, LOAD_B } res_station_t;

// execution units types
typedef enum { INTEGER, ADDER, MULTIPLIER, DIVIDER, MEMORY } exe_unit_t;

// stages names
typedef enum { ISSUE, EXECUTE, WRITE_RESULT, COMMIT } stage_t;

// instruction data type
typedef struct {
  /*opcode*/
  opcode_t opcode;
  /*first source register in the assembly instruction (for SW,register to be
   * written to memory)*/
  unsigned src1;
  /*second source register in the assembly instruction*/
  unsigned src2;
  /*destination register*/
  unsigned dest;
  /*immediate field*/
  unsigned immediate;
  /*for conditional branches, label of the target instruction -used only for
   * parsing/debugging purposes*/
  string label;
} instruction_t;

// execution unit
typedef struct {
  // execution unit type
  exe_unit_t type;
  // execution unit latency
  unsigned latency;
  /* 0 if execution unit is free, otherwise number of clock cycles during which
   * the execution unit will be busy. It should be initialized to the latency of
   * the unit when the becomes busy, and decremented at each clock cycle*/
  unsigned busy;
  /*PC of the instruction using the functional unit*/
  unsigned pc;
} unit_t;

// entry in the "instruction window"
typedef struct {
  unsigned pc;     // PC of the instruction
  unsigned issue;  // clock cycle when the instruction is issued
  unsigned exe;    // clock cycle when the instruction enters execution
  unsigned wr;     // clock cycle when the instruction enters write result
  unsigned commit; // clock cycle when the instruction commits (for stores,
                   // clock cycle when the store starts committing
} instr_window_entry_t;

// Re Order Buffer
// ROB entry
typedef struct {
  bool ready;  // ready field
  unsigned pc; // pc of corresponding instruction (set to UNDEFINED if ROB entry
  // is available)
  stage_t state;        // state field
  unsigned destination; // destination field
  unsigned value;       // value field
  bool empty;
} rob_entry_t;

// ROB
typedef struct {
  unsigned num_entries;
  rob_entry_t *entries;
} rob_t;
// reservation station entry
typedef struct {
  res_station_t type;   // reservation station type
  unsigned name;        // reservation station name (i.e., "Int", "Add", "Mult",
                        // "Load") for logging purposes
  unsigned pc;          // pc of corresponding instruction (set to UNDEFINED if
                        // reservation station is available)
  unsigned value1;      // Vj field
  unsigned value2;      // Vk field
  unsigned tag1;        // Qj field
  unsigned tag2;        // Qk field
  unsigned destination; // destination field
  unsigned address;     // address field (for loads and stores)
} res_station_entry_t;

// integer register entry
typedef struct {
  int value;
  unsigned name;
  bool busy;
} int_gp_reg_entry;

// floating point register entry
typedef struct {
  float value;
  unsigned name;
  bool busy;
} float_gp_reg_entry;

static int_gp_reg_entry null_int_register_entry = {0xff, UNDEFINED, false};

static float_gp_reg_entry null_float_register_entry = {0xff, UNDEFINED, false};

// instruction window
typedef struct {
  unsigned num_entries;
  instr_window_entry_t *entries;
} instr_window_t;

// reservation stations
typedef struct {
  unsigned num_entries;
  res_station_entry_t *entries;
} res_stations_t;


typedef struct {
  //indexes
  unsigned robIndex;
  unsigned resStationIndex;
  unsigned instrWindowIndex;
  unsigned executionUnitNumber;
  unsigned instrMemoryIndex;
 //opcode
  opcode_t instructionOpcode;
  //res station type
  res_station_t resStationType;
  //clock helpers
  bool readyToWrite;
  unsigned resStationAvailable;
  unsigned valuesAvailable;
  bool readyToCommit;
} map_entry_t;

class sim_ooo {

  /* Add the data members required by your simulator's implementation here */
  // gp integer registers
  int_gp_reg_entry int_gp[NUM_GP_REGISTERS];

  // gp floating point registers
  float_gp_reg_entry float_gp[NUM_GP_REGISTERS];
  /* end added data members */

  // issue width
  unsigned issue_width;

  // instruction window
  instr_window_t pending_instructions;

  // reorder buffer
  rob_t rob;

  // reservation stations
  res_stations_t reservation_stations;

  // execution units
  unit_t exec_units[MAX_UNITS];

  unsigned num_units;

  // instruction memory
  instruction_t instr_memory[PROGRAM_SIZE];

  // base address in the instruction memory where the program is loaded
  unsigned instr_base_address;

  // data memory - should be initialize to all 0xFF
  unsigned char *data_memory;

  // memory size in bytes
  unsigned data_memory_size;

  // instructions executed
  unsigned instructions_executed;

  // clock cycles
  unsigned clock_cycles;

  // execution log
  stringstream log;

  // queue of rob entries
  queue<unsigned> robq;

  queue<unsigned> robs_to_clear;

  queue<unsigned> res_stations_to_clear;

  queue<unsigned> instr_windows_to_clear;

  queue<unsigned> res_stations_to_update;

  queue<unsigned> branch_instruction_map_keys;
  // link between rob, reservation station, and instruction window
  map<unsigned, map_entry_t> instruction_map;



public:
  /* Instantiates the simulator
          Note: registers must be initialized to UNDEFINED value, and data
     memory to all 0xFF values
  */
  sim_ooo(
      unsigned mem_size,             // size of data memory (in byte)
      unsigned rob_size,             // number of ROB entries
      unsigned num_int_res_stations, // number of integer reservation stations
      unsigned num_add_res_stations, // number of ADD reservation stations
      unsigned num_mul_res_stations, // number of MULT/DIV reservation stations
      unsigned num_load_buffers,     // number of LOAD buffers
      unsigned issue_width = 1       // issue width
  );

  // de-allocates the simulator
  ~sim_ooo();

  // adds one or more execution units of a given type to the processor
  // - exec_unit: type of execution unit to be added
  // - latency: latency of the execution unit (in clock cycles)
  // - instances: number of execution units of this type to be added
  void init_exec_unit(exe_unit_t exec_unit, unsigned latency,
                      unsigned instances = 1);

  // related to functional unit
  unsigned get_free_unit(opcode_t opcode);

  // loads the assembly program in file "filename" in instruction memory at the
  // specified address
  void load_program(const char *filename, unsigned base_address = 0x0);

  // runs the simulator for "cycles" clock cycles (run the program to completion
  // if cycles=0)
  void run(unsigned cycles = 0);

  void a_cycle();
  // resets the state of the simulator
  /* Note:
     - registers should be reset to UNDEFINED value
     - data memory should be reset to all 0xFF values
     - instruction window, reservation stations and rob should be cleaned
  */
  void reset();

  // returns value of the specified integer general purpose register
  int get_int_register(unsigned reg);

  // set the value of the given integer general purpose register to "value"
  void set_int_register(unsigned reg, int value);

  // returns value of the specified floating point general purpose register
  float get_fp_register(unsigned reg);

  // set the value of the given floating point general purpose register to
  // "value"
  void set_fp_register(unsigned reg, float value);

  // returns the index of the ROB entry that will write this integer register
  // (UNDEFINED if the value of the register is not pending
  unsigned get_int_register_tag(unsigned reg);

  // returns the index of the ROB entry that will write this floating point
  // register (UNDEFINED if the value of the register is not pending
  unsigned get_fp_register_tag(unsigned reg);

  // returns the IPC
  float get_IPC();

  // returns the number of instructions fully executed
  unsigned get_instructions_executed();

  // returns the number of clock cycles
  unsigned get_clock_cycles();

  // prints the content of the data memory within the specified address range
  void print_memory(unsigned start_address, unsigned end_address);

  // writes an integer value to data memory at the specified address (use
  // little-endian format: https://en.wikipedia.org/wiki/Endianness)
  void write_memory(unsigned address, unsigned value);

  // prints the values of the registers
  void print_registers();

  // prints the status of processor excluding memory
  void print_status();

  // prints the content of the ROB
  void print_rob();

  // prints the content of the reservation stations
  void print_reservation_stations();

  // print the content of the instruction window
  void print_pending_instructions();

  void print_pc_to_instruction(unsigned thisPC);

  // initialize the execution log
  void init_log();

  // commit an instruction to the log
  void commit_to_log(instr_window_entry_t iwe);

  // print log
  void print_log();

  // fp or integer instruction

  bool is_fp_instruction(opcode_t thisOpcode);

  // set float register entry to null
  void nullify_float_reg_entry(float_gp_reg_entry &thisEntry);

  // set int register entry to null
  void nullify_int_reg_entry(int_gp_reg_entry &thisEntry);

  // change some float register field value
  void float_reg_set(float_gp_reg_entry &thisEntry, float thisValue = 0xff,
                     unsigned thisName = UNDEFINED, bool toggle = false);

  // change some int register field value
  void int_reg_set(float_gp_reg_entry &thisEntry, int thisValue = 0xff,
                   unsigned thisName = UNDEFINED, bool toggle = false);

  /////////////////////
  // ISSUE FUNCTIONS //
  /////////////////////
  unsigned mem_to_index(unsigned thisMemoryValue);

  void issue_instruction();

  // decode instruction in issue stage
  void issue_decode(instruction_t thisInstruction);

  // conditional check
  bool is_conditional(opcode_t thisOpcode);

    void station_delay_check(res_station_t thisReservationStation);
  ///////////////////
  // ROB FUNCTIONS //
  ///////////////////

  // function to determine if there is an empty slot in the reorder buffer
  bool rob_full();

  unsigned rob_add(instruction_t &thisInstruction, unsigned thisPC);

  //////////////////////////////////////
  // RESERVATION STATION FUNCTIONS    //
  //////////////////////////////////////
  // function to determine if src value should be added as tag or value in
  // reservation station;
  unsigned pending_int_src_check(unsigned int thisSource);

  res_station_t get_station_type(instruction_t thisInstruction);

  unsigned get_available_res_station(res_station_t thisTypeOfStation);

  void reservation_station_stats();

  void reservation_station_check();

  void reservation_station_add(map_entry_t thisMapEntry,
                               unsigned pcOfInstruction);

  unsigned get_tag(unsigned thisReg);

  opcode_t pc_to_opcode_type(unsigned thisPC);

  void clear_reservation_station(unsigned thisReservationStation);

  void two_argument_tag_check(map_entry_t thisMapEntry);

  void single_argument_tag_check(map_entry_t thisMapEntry);

  void load_argument_tag_check(map_entry_t thisMapEntry);

  void store_argument_tag_check(map_entry_t thisMapEntry);

  void conditional_argument_tag_check(map_entry_t thisMapEntry);

  unsigned find_value_in_rob(unsigned thisRobEntry);

  unsigned find_tag_in_rob(unsigned thisRegister);

  bool is_load_instruction(opcode_t thisOpcode);

  bool is_store_instruction(opcode_t thisOpcode);

  /////////////////////////////////////////////
  // CHECKING RESERVATION STATION ARGUMENTS  //
  /////////////////////////////////////////////
  bool arguments_ready_find(unsigned thisPC);

  bool arguments_ready_load(unsigned res_station_index);

  bool arguments_ready_store(unsigned res_station_index);

  bool arguments_ready_int_imm(unsigned res_station_index);

  bool arguments_ready_fp_alu(unsigned res_station_index);

  bool arguments_ready_conditional(unsigned res_station_index);
  unsigned get_res_station_index(unsigned thisPC);

  //////////////////////////////////
  // INSTRUCTION WINDOW FUNCTIONS //
  //////////////////////////////////
  void instruction_window_add(unsigned thisWindowIndex,unsigned thisPC);

  void instruction_window_set_clock(unsigned thisPC);

  void instruction_window_remove(unsigned thisInstructionWindowIndex);

  /////////////////////////
  // EXECUTION FUNCTIONS //
  /////////////////////////
  void execute();

  void find_pending_execution_instructions();

  void find_available_execution_unit(unsigned thisPC);

  void claim_execution_unit(unsigned thisUnit, unsigned thisPC);

  void cycle_execution_units();

  void print_map_entry(unsigned thisKeyValue);

  void print_map_entry(map_entry_t thisMapEntry);

  void take_instruction_action(map_entry_t thisMapEntry);

  void fp_instruction_action(map_entry_t thisMapEntry);

  void int_imm_instruction_action(map_entry_t thisMapEntry);

  void print_cycle_info(map_entry_t thisMapEntry);
  // preamble
  void int_instruction_action(map_entry_t thisMapEntry);
  // preamble
  void conditional_instruction_action(map_entry_t thisMapEntry);

  void print_active_execution_units();

  void eop_execution();
  //////////////////////
  // MEMORY FUNCTIONS //
  //////////////////////
  unsigned get_unsigned_memory_value(unsigned thisIndexOfMemory);

  void lws_instruction_action(map_entry_t thisMapEntry);

  /////////////////////////////
  // WRITE RESULT FUNCTIONS  //
  /////////////////////////////
  void write_results();

  void find_ready_to_write();

  void update_reservation_stations(unsigned thisResStationIndex);
  /////////////////////////////
  // COMMIT FUNCTIONS        //
  /////////////////////////////
  // run function call
  void commit();
  // find next instruction that's ready to commit
  void commit_find();
  // perform the committing
  void commit_commit(bool isFloat, unsigned thisRegister, unsigned thisValue);

  void clear_rob_entry(unsigned thisRobEntry);

  /////////////////////////////
  // POST PROCESS FUNCTIONS  //
  /////////////////////////////

  void post_process();
  void clear_res_station(unsigned thisResStationIndex);
};
// printing which value is impeding execution
void print_culprit(unsigned thisVal1 = 0, unsigned thisVal2 = 0);
// printing memory information (load and store instruction)
void print_memory_update(unsigned thisValue, unsigned thisOffset,
                         unsigned thisMemIndex, unsigned thisConvertedValue);
// printing executiion unit type as a name instead of an integer
void print_string_unit_type(exe_unit_t thisUnit);
// printing the write result status and information
void print_write_results(unsigned statement, map_entry_t thisMapEntry);

void print_string_opcode(opcode_t thisOpcode);

void print_commit_init();

void print_write_status(bool writeStatus);
#endif /*SIM_OOO_H_*/
