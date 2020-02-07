#include "sim_pipe.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

//#define DEBUG

using namespace std;

// used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {
    "PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {
    "LW",   "SW",   "ADD",  "ADDI", "SUB",  "SUBI", "XOR", "BEQZ",
    "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP"};

/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */

/* converts integer into array of unsigned char - little indian */
inline void int2char(unsigned value, unsigned char *buffer) {
  memcpy(buffer, &value, sizeof value);
}

/* converts array of char into integer - little indian */
inline unsigned char2int(unsigned char *buffer) {
  unsigned d;
  memcpy(&d, buffer, sizeof d);
  return d;
}

/* implements the ALU operations */
unsigned alu(unsigned opcode, unsigned a, unsigned b, unsigned imm,
             unsigned npc) {
  switch (opcode) {
  case ADD:
    return (a + b);
  case ADDI:
    return (a + imm);
  case SUB:
    return (a - b);
  case SUBI:
    return (a - imm);
  case XOR:
    return (a ^ b);
  case LW:
  case SW:
    return (a + imm);
  case BEQZ:
  case BNEZ:
  case BGTZ:
  case BGEZ:
  case BLTZ:
  case BLEZ:
  case JUMP:
    return (npc + imm);
  default:
    return (-1);
  }
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* loads the assembly program in file "filename" in instruction memory at the
 * specified address */
void load_program(const char *filename, unsigned base_address) {

  /* initializing the base instruction address */
  mips.instr_base_address = base_address;

  /* creating a map with the valid opcodes and with the valid labels */
  map<string, opcode_t> opcodes; // for opcodes
  map<string, unsigned> labels;  // for branches
  for (int i = 0; i < NUM_OPCODES; i++)
    opcodes[string(instr_names[i])] = (opcode_t)i;

  /* opening the assembly file */
  ifstream fin(filename, ios::in | ios::binary);
  if (!fin.is_open()) {
    cerr << "error: open file " << filename << " failed!" << endl;
    exit(-1);
  }

  /* parsing the assembly file line by line */
  string line;
  unsigned instruction_nr = 0;
  while (getline(fin, line)) {
    // set the instruction field
    char *str = const_cast<char *>(line.c_str());

    // tokenize the instruction
    char *token = strtok(str, " \t");
    map<string, opcode_t>::iterator search = opcodes.find(token);
    if (search == opcodes.end()) {
      // this is a label for a branch - extract it and save it in the labels map
      string label = string(token).substr(0, string(token).length() - 1);
      labels[label] = instruction_nr;
      // move to next token, which must be the instruction opcode
      token = strtok(NULL, " \t");
      search = opcodes.find(token);
      if (search == opcodes.end())
        cout << "ERROR: invalid opcode: " << token << " !" << endl;
    }
    mips.instr_memory[instruction_nr].opcode = search->second;

    // reading remaining parameters
    char *par1;
    char *par2;
    char *par3;
    switch (mips.instr_memory[instruction_nr].opcode) {
    case ADD:
    case SUB:
    case XOR:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      par3 = strtok(NULL, " \t");
      mips.instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
      mips.instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
      mips.instr_memory[instruction_nr].src2 = atoi(strtok(par3, "R"));
      break;
    case ADDI:
    case SUBI:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      par3 = strtok(NULL, " \t");
      mips.instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
      mips.instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
      mips.instr_memory[instruction_nr].immediate = strtoul(par3, NULL, 0);
      break;
    case LW:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      mips.instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
      mips.instr_memory[instruction_nr].immediate =
          strtoul(strtok(par2, "()"), NULL, 0);
      mips.instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
      break;
    case SW:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      mips.instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
      mips.instr_memory[instruction_nr].immediate =
          strtoul(strtok(par2, "()"), NULL, 0);
      mips.instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
      break;
    case BEQZ:
    case BNEZ:
    case BLTZ:
    case BGTZ:
    case BLEZ:
    case BGEZ:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      mips.instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
      mips.instr_memory[instruction_nr].label = par2;
      break;
    case JUMP:
      par2 = strtok(NULL, " \t");
      mips.instr_memory[instruction_nr].label = par2;
    default:
      break;
    }

    /* increment instruction number before moving to next line */
    instruction_nr++;
  }
  // reconstructing the labels of the branch operations
  int i = 0;
  while (true) {
    instruction_t instr = mips.instr_memory[i];
    if (instr.opcode == EOP)
      break;
    if (instr.opcode == BLTZ || instr.opcode == BNEZ || instr.opcode == BGTZ ||
        instr.opcode == BEQZ || instr.opcode == BGEZ || instr.opcode == BLEZ ||
        instr.opcode == JUMP) {
      mips.instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
    }
    i++;
  }
}

/* writes an integer value to data memory at the specified address (use
 * little-endian format: https://en.wikipedia.org/wiki/Endianness) */
void write_memory(unsigned address, unsigned value) {
  int2char(value, mips.data_memory + address);
}

/* prints the content of the data memory within the specified address range */
void print_memory(unsigned start_address, unsigned end_address) {
  cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address
       << ":0x" << hex << setw(8) << setfill('0') << end_address << "]" << endl;
  for (unsigned i = start_address; i < end_address; i++) {
    if (i % 4 == 0)
      cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
    cout << hex << setw(2) << setfill('0') << int(mips.data_memory[i]) << " ";
    if (i % 4 == 3)
      cout << endl;
  }
}

/* prints the values of the registers */
void print_registers() {
  cout << "Special purpose registers:" << endl;
  unsigned i, s;
  for (s = 0; s < NUM_STAGES; s++) {
    cout << "Stage: " << stage_names[s] << endl;
    for (i = 0; i < NUM_SP_REGISTERS; i++)
      if ((sp_register_t)i != IR && (sp_register_t)i != COND &&
          get_sp_register((sp_register_t)i, (stage_t)s) != UNDEFINED)
        cout << reg_names[i] << " = " << dec
             << get_sp_register((sp_register_t)i, (stage_t)s) << hex << " / 0x"
             << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
  }
  cout << "General purpose registers:" << endl;
  for (i = 0; i < NUM_GP_REGISTERS; i++)
    if (get_gp_register(i) != (int)UNDEFINED)
      cout << "R" << dec << i << " = " << get_gp_register(i) << hex << " / 0x"
           << get_gp_register(i) << endl;
}

/* initializes the pipeline simulator */
void sim_pipe_init(unsigned mem_size, unsigned mem_latency) {
  mips.data_memory_size = mem_size;
  mips.data_memory_latency = mem_latency;
  mips.data_memory = new unsigned char[mips.data_memory_size];
  reset();
}

/* deallocates the pipeline simulator */
void sim_pipe_terminate() { delete[] mips.data_memory; }

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */

/* body of the simulator */
void run(unsigned cycles) {
  /* If cycles has argument, run for that number of cycles
   * if not, run asm to completion*/
  /* A full run for each instruction will look like the following
   * at clock cycle 1:
   * Fetch instruction, pass to IR of IF_ID stage
   */

  unsigned cyclesRan = 0;
  switch (cycles) {
  case NOT_DECLARED:
    while (mips.pipeline[EXE_MEM].intruction_register.opcode != EOP) {
      // keep moving forward
    }
    break;
  default:
    while (cyclesRan < cycles) {
      switch (cyclesRan) {
      case 0:
        // run_fetch()
        mips.pipeline[IF_ID].intruction_register = mips.instr_memory[0];
        mips.pipeline[IF_ID].SP_REGISTERS[PC] = mips.instr_base_address + 4;
        mips.pipeline[IF_ID].SP_REGISTERS[NPC] = mips.instr_base_address + 4;
        // get npc
        break;
      case 1:
        // run_decode()
        /* get 'A' value, get 'B' value, if necesarry, get 'IMM' value */
        break;
      case 2:
        // run_execute()
        break;
      case 3:
        // run_memory()
        break;
        // run_write_back()
      case 4:
        break;
      }
      cyclesRan++;
    }
    break;
  }
  // get instruction first instruction memory
  // Instruction fetch
  // Instruction decode
  // Instruction execute
  // Instruction memory
  // Instruction write back
  //
}

/* reset the state of the pipeline simulator */
void reset() {
  clear_registers();
  clear_memory();
}

// return value of special purpose register
/*
 *Question:
 *the sp register of a particular stage
 *for stage IF -> what sp registers exist at this point?
 *does IF_ID
 */
unsigned get_sp_register(sp_register_t reg, stage_t s) {

  return mips.pipeline[s].SP_REGISTERS[reg];
}

// returns value of general purpose register
int get_gp_register(unsigned reg) {
  return mips.GP_Registers[reg]; // please modify
}

void set_gp_register(unsigned reg, int value) {
  mips.GP_Registers[reg] = value;
}

float get_IPC() {
  return 0; // please modify
}

unsigned get_instructions_executed() {
  return 0; // please modify
}

unsigned get_stalls() {
  return 0; // please modify
}

unsigned get_clock_cycles() {
  return 0; // please modify
}
void clear_registers() {
  for (int i = 0; i < NUM_GP_REGISTERS; i++) {
    mips.GP_Registers[i] = UNDEFINED;
  }
  for (int i = 0; i < NUM_SP_REGISTERS; i++) {
  }
}
void clear_memory() {
  for (int i = 0; i < PROGRAM_SIZE; i++) {
    mips.data_memory[PROGRAM_SIZE] = 0xFF;
  }
}
