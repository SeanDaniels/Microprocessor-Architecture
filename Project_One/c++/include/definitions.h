#ifndef DEFINITIONS_H
#define DEFINITIONS_H
#include <stdio.h>
#include <string>

using namespace std;


#define UNDEFINED 0xFFFFFFFF
#define ZERO      0x00000000
#define PROGRAM_SIZE 50
#define NUM_SP_REGISTERS 9
#define NUM_GP_REGISTERS 32
#define NUM_OPCODES 22
#define NUM_STAGES 5

typedef enum {IF, ID, EXE, MEM, WB} stage_t;
typedef enum {PC, NPC, IR, A, B, IMM, COND, ALU_OUTPUT, LMD} sp_register_t;
typedef enum {LW, SW, ADD, ADDI, SUB, SUBI, XOR, BEQZ, BNEZ, BLTZ, BGTZ, BLEZ, BGEZ, JUMP, EOP, NOP, LWS, SWS, ADDS, SUBS, MULTS, DIVS} opcode_t;
typedef enum {INTEGER, ADDER, MULTIPLIER, DIVIDER} exe_unit_t;

// instruction
typedef struct{
  opcode_t opcode; //opcode
  unsigned src1; //first source register
  unsigned src2; //second source register
  unsigned dest; //destination register
  unsigned immediate; //immediate field;
  string label; //in case of branch, label of the target instruction;
	string instruction; //instruction processed
  exe_unit_t type;
} instruction_t;

// execution unit
typedef struct{
	 // execution unit type
	unsigned latency; // execution unit latency
	unsigned busy;
  // 0 if execution unit is free, otherwise number of clock cycles during
  // which the execution unit will be busy. It should be initialized
  // to the latency of the unit when the unit becomes busy, and decremented
  // at each clock cycle
	instruction_t instruction; // instruction using the functional unit
} unit_t;

typedef struct{
  unsigned PC;
  unsigned WE;
} fetch_registers;

typedef struct{
  unsigned NPC;
  instruction_t IR;
  unsigned WE;
} pipe_stage_one;

typedef struct{
  unsigned A;
  unsigned B;
  unsigned IMM;
  unsigned NPC;
  instruction_t IR;
  unsigned WE;
} pipe_stage_two;

typedef struct{
  unsigned ALU_out;
  unsigned B;
  unsigned condition;
  instruction_t IR;
  unsigned WE;
} pipe_stage_three;

typedef struct{
  unsigned LMD;
  instruction_t IR;
  unsigned ALU_out;
  unsigned condition;
  unsigned WE;
} pipe_stage_four;

#endif
