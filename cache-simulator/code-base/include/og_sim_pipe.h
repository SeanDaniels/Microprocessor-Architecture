#ifndef SIM_PIPE_H_
#define SIM_PIPE_H_

#include <stdio.h>
#include <string>
#include "cache.h"

using namespace std;

#define PROGRAM_SIZE 50

#define UNDEFINED 0xFFFFFFFF // used to initialize the registers
#define NUM_SP_REGISTERS 9
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 16
#define NUM_STAGES 5
#define MY_NUM_STAGES 6
#define NUM_RUN_FUNCTIONS 8
#define CYCLES_NOT_DECLARED NULL
#define BRANCH_STALLS 2

typedef enum { PC, NPC, IR, A, B, IMM, COND, ALU_OUTPUT, LMD } sp_register_t;

typedef enum {IF_R,B_IF_R,ID_R,L_ID_R,EXE_R,MEM_R,S_MEM_R,WB_R} run_functions_t;

typedef enum {
LW,
SW,
ADD,
ADDI,
SUB,
SUBI,
XOR,
BEQZ,
BNEZ,
BLTZ,
BGTZ,
BLEZ,
BGEZ,
JUMP,
EOP,
NOP
} opcode_t;

typedef enum { IF, ID, EXE, MEM, WB } stage_t;

typedef enum {IF_M,ID_M,EXE_M,MEM_M,WB_M,LD_M} my_stage_t;

typedef enum {PRE_FETCH,IF_ID,ID_EXE,EXE_MEM,MEM_WB} pipeline_stage_t;

typedef enum {PIPELINE_PC} pre_fetch_stage_t;

typedef enum {IF_ID_NPC} fetch_decode_stage_t;

typedef enum {ID_EXE_A, ID_EXE_B, ID_EXE_IMM, ID_EXE_NPC} decode_execute_stage_t;

typedef enum {EXE_MEM_B, EXE_MEM_ALU_OUT, EXE_MEM_COND} execute_memory_stage_t;

typedef enum {MEM_WB_ALU_OUT, MEM_WB_LMD} memory_writeback_stage_t;

typedef enum {ARITH_INSTR, COND_INSTR, LWSW_INSTR, NOPEOP_INSTR} kind_of_instruction_t;

typedef struct {
  opcode_t opcode; // opcode
  unsigned src1;   // first source register in the assembly instruction (for SW,
  // register to be written to memory)
  unsigned src2;      // second source register in the assembly instruction
  unsigned dest;      // destination register
  unsigned immediate; // immediate field
  string label; // for conditional branches, label of the target instruction -
                // used only for parsing/debugging purposes
} instruction_t;


typedef struct {
  instruction_t parsedInstruction;
  unsigned spRegisters[NUM_SP_REGISTERS] = {UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED};
} pipeline_sp_t;

typedef struct {
  pipeline_sp_t stage[NUM_STAGES];
} pipeline_t;

class sim_pipe {

  /* Add the data members required by your simulator's implementation here */

  // instruction memory
  instruction_t instr_memory[PROGRAM_SIZE];

  // base address in the instruction memory where the program is loaded
  unsigned instr_base_address;

  // data memory - should be initialize to all 0xFF
  unsigned char *data_memory;

  // memory size in bytes
  unsigned data_memory_size;

  // memory latency in clock cycles
  unsigned data_memory_latency;

/* pipeline object */
  pipeline_t pipeline;

/*int array of registers*/
  int gp_registers[NUM_GP_REGISTERS];

/*number of stalls*/
  unsigned stalls = 0;

/*number of clock cycles */
  float clock_cycles = 0;

/*number of instructions executeds */
  float instructions_executed = 0;

  bool program_complete = false;

  cache* memory_cache;
  public:
    // instantiates the simulator with a data memory of given size (in bytes) and
    // latency (in clock cycles)
    /* Note:
       - initialize the registers to UNDEFINED value
       - initialize the data memory to all 0xFF values
    */
    sim_pipe(unsigned data_mem_size, unsigned data_mem_latency);
  

    // de-allocates the simulator
    ~sim_pipe();

    // loads the assembly program in file "filename" in instruction memory at the
    // specified address
    void load_program(const char *filename, unsigned base_address = 0x0);

    // runs the simulator for "cycles" clock cycles (run the program to completion
    // if cycles=0)

    // resets the state of the simulator
    /* Note:
       - registers should be reset to UNDEFINED value
       - data memory should be reset to all 0xFF values
    */
    void reset();

    // returns value of the specified special purpose register for a given stage
    // (at the "entrance" of that stage) if that special purpose register is not
    // used in that stage, returns UNDEFINED
    //
    // Examples (refer to page C-37 in the 5th edition textbook, A-32 in 4th
    // edition of textbook)::
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

    // returns value of the specified general purpose register
    int get_gp_register(unsigned reg);

    // set the value of the given general purpose register to "value"
    void set_gp_register(unsigned reg, int value);

    // returns the IPC
    float get_IPC();

    // returns the number of instructions fully executed
    unsigned get_instructions_executed();

    // returns the number of clock cycles
    unsigned get_clock_cycles();

    // returns the number of stalls added by processor
    unsigned get_stalls();

    // prints the content of the data memory within the specified address range
    void print_memory(unsigned start_address, unsigned end_address);

    // writes an integer value to data memory at the specified address (use
    // little-endian format: https://en.wikipedia.org/wiki/Endianness)
    void write_memory(unsigned address, unsigned value);

    // prints the values of the registers
    void print_registers();

///////////////////////
// CONTROL FUNCTIONS //
///////////////////////
    void run(unsigned cycles = 0);

    void run_clock();
/////////////////////
// FETCH FUNCTIONS //
/////////////////////

    void fetch();

    void branch_fetch();

//////////////////////
// DECODE FUNCTIONS //
//////////////////////

    /*function to determine which type of decode needs to be done*/
    void decode();
    /*function to handle a decode when the pipeline isn't locked*/
    void normal_decode(instruction_t currentInstruction);
    /*function to handle a decode when the pipeline is locked*/
    void lock_decode();

    bool single_source_check(instruction_t checkInstruction);
    /*function to determine if a data dep exists*/
    int data_dep_check(instruction_t checkedInstruction);

    int stage_location(opcode_t checkOpcode);

///////////////////////
// EXECUTE FUNCTIONS //
///////////////////////

    void execute();

    unsigned conditional_evaluation(unsigned evaluate, opcode_t condition);

//////////////////////
// MEMORY FUNCTIONS //
//////////////////////

    void memory();

    void memory_stall();

    void memory_store(unsigned thisB, unsigned thisALUOutput);

    void memory_load(unsigned thisALUOutput);

//////////////////////////
// WRITE BACK FUNCTIONS //
//////////////////////////

    void write_back();

//////////////////////
// HELPER FUNCTIONS //
//////////////////////

    void processor_key_update();

    void set_program_complete();

    bool get_program_complete();
    /*function to determine what kind of instruction is being processed*/
    kind_of_instruction_t
    instruction_type_check(instruction_t checkedInstruction);
    /*function to handle branch NOP insertions*/

    void set_sp_reg(pipeline_stage_t thisStage, int reg, unsigned registerVal);

    void set_sp_reg_instruction(pipeline_stage_t thisStage, instruction_t thisInstruction);

    instruction_t get_sp_reg_instruction(pipeline_stage_t thisStage,
                                         instruction_t thisInstruction);

    /* function to insert a stall into the pipeline, called from a stage, pushed to next stage */
    void insert_stall(pipeline_stage_t nextStage);

    void set_cache(cache *c);


};

#endif /*SIM_PIPE_H_*/
