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

//global processor imperatives
int decodeNeeded = 0;
int executeNeeded = 0;
int memoryNeeded = 0;
int writeBackNeeded = 0;
int processorKey[5] = {1,0,0,0,0};
int processorKeyNext[5] = {0,0,0,0,0};
unsigned instructionsExecuted = 0;
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
void fetch() {
  /*Function to get the next instruction
   *Next instruction will be provided by NPC of EXE_MEM Pipeline from run
   *or will default to zero (first instruction)
   *
   */
   //if current instruction is branch, next instruction needs to wait in decode
   //until this instruction is is ex_mem stage
  if (mips.instr_memory[fetchInstruction].opcode != EOP) {
    // get instruction
    mips.pipeline[IF_ID].intruction_register =
        mips.instr_memory[fetchInstruction];
    // Push values to sp registers
    mips.pipeline[IF_ID].SP_REGISTERS[PC] = fetchInstruction;
    if (mips.pipeline[EXE_MEM].SP_REGISTERS[COND] == 1) {
      // NPC = should be the branch target pretty sure this is the ALU output of
      // previous instruction
      mips.pipeline[IF_ID].SP_REGISTERS[NPC] =
          mips.pipeline[EXE_MEM].SP_REGISTERS[ALU_OUTPUT];
    } else {
      // follow normal piping procedures
      mips.pipeline[IF_ID].SP_REGISTERS[NPC] = fetchInstruction + 4;
    }
    // update control array
    processorKeyNext[1]++;
  } else
    processorKeyNext[0] = 0;
}
void decode() {
  /*Function to parse the register file into special purpose registers
   */
   //forward instruction register from ID_EXE stage
  mips.pipeline[ID_EXE].intruction_register =
      mips.pipeline[IF_ID].intruction_register;
  // get register A
  mips.pipeline[ID_EXE].SP_REGISTERS[A] =
      mips.pipeline[IF_ID].intruction_register.src1;
  // get register B
  mips.pipeline[ID_EXE].SP_REGISTERS[B] =
      mips.pipeline[IF_ID].intruction_register.src2;
  // get immediate register
  mips.pipeline[ID_EXE].SP_REGISTERS[IMM] =
    mips.pipeline[IF_ID].intruction_register.immediate;
  // get NPC register
  mips.pipeline[ID_EXE].SP_REGISTERS[NPC] =
      mips.pipeline[IF_ID].SP_REGISTERS[NPC];
  //decrement number of decodes stages needed
  processorKeyNext[1]--;
  //increment number of execute stages needed
  processorKeyNext[2]++;
}
void execute(){
  /*
   *Conditional logic to determine what happens in execution unit
   *What are arguments of ALU:
   *If opcode is ADD, SUB, XOR, *LW, *SW:
   *Arguments are A and B (*Potentially + some offset)
   */
   //Get conditional
  if (mips.pipeline[ID_EXE].intruction_register.opcode == BEQZ ||
      mips.pipeline[ID_EXE].intruction_register.opcode == BNEZ ||
      mips.pipeline[ID_EXE].intruction_register.opcode == BLTZ ||
      mips.pipeline[ID_EXE].intruction_register.opcode == BGTZ ||
      mips.pipeline[ID_EXE].intruction_register.opcode == BLEZ ||
      mips.pipeline[ID_EXE].intruction_register.opcode == BGEZ ) {
    //we have a conditional
  }
  switch(mips.pipeline[ID_EXE].intruction_register.opcode){
    case BEQZ:
      if(mips.pipeline[ID_EXE].SP_REGISTERS[A]==0){
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 1;
      }
      break;
    case BNEZ:
      if(mips.pipeline[ID_EXE].SP_REGISTERS[A]!=0){
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 1;
      }
      break;
    case BLTZ:
      if(mips.pipeline[ID_EXE].SP_REGISTERS[A]<0){
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 1;
      }
      break;
    case BGTZ:
      if(mips.pipeline[ID_EXE].SP_REGISTERS[A]>0){
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 1;
      }
      break;
    case BLEZ:
      if(mips.pipeline[ID_EXE].SP_REGISTERS[A]<=0){
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 1;
      }
      break;
    case BGEZ:
      if(mips.pipeline[ID_EXE].SP_REGISTERS[A]>=0){
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 1;
      }
      break;
    default:
      mips.pipeline[EXE_MEM].SP_REGISTERS[ALU_OUTPUT] =
          alu(mips.pipeline[ID_EXE].intruction_register.opcode,
              mips.pipeline[ID_EXE].SP_REGISTERS[A],
              mips.pipeline[ID_EXE].SP_REGISTERS[B],
              mips.pipeline[ID_EXE].SP_REGISTERS[IMM],
              mips.pipeline[ID_EXE].SP_REGISTERS[NPC]);
        mips.pipeline[EXE_MEM].SP_REGISTERS[COND] = 0;
      break;
    }
   //take ALU action
  // decrement number of execute stages needed
  processorKeyNext[2]--;
  // increment number of memory stages needed
  processorKeyNext[3]++;
}
void memory() {
  //load memory from register
  //propogate IR
  mips.pipeline[MEM_WB].intruction_register =
      mips.pipeline[EXE_MEM].intruction_register;
  // propogate ALU output
  mips.pipeline[MEM_WB].SP_REGISTERS[ALU_OUTPUT] =
      mips.pipeline[EXE_MEM].SP_REGISTERS[ALU_OUTPUT];
  if(mips.pipeline[EXE_MEM].intruction_register.opcode == LW ||mips.pipeline[EXE_MEM].intruction_register.opcode == SW ){
    //store memory or load memory
    if(mips.pipeline[EXE_MEM].intruction_register.opcode == LW){
      //go to address stored passed from pipeline
      //write info from address to other address passed from pipeline
      //pass that to MEM_WB pipeline
      //mips.pipeline[MEM_WB].SP_REGISTERS[LMD] = the data fetch
    } else {
      //here you are storing
      //get value in register passed from pipeline, i think B
      //write that value to address in a
    }
  }

  // decrement number of memory stages needed
  processorKeyNext[3]--;
  //Conditionally increment number of write back stages needed
  processorKeyNext[4]++;
}
void write_back() {
  //put whats in the alu output register into the destination that
  //is in the destination register
  //if load instruction, pass LMD to register
  //if arithmatic, pass alu output
  //either store word or load word happens in the memory stage, not sure which one
  if (mips.pipeline[MEM_WB].intruction_register.opcode == LW ||
      mips.pipeline[MEM_WB].intruction_register.opcode == ADD ||
      mips.pipeline[MEM_WB].intruction_register.opcode == ADDI ||
      mips.pipeline[MEM_WB].intruction_register.opcode == SUBI ||
      mips.pipeline[MEM_WB].intruction_register.opcode == XOR) {
    if (mips.pipeline[MEM_WB].intruction_register.opcode != LW) {
      mips.pipeline[MEM_WB].intruction_register.dest =
          mips.pipeline[MEM_WB].SP_REGISTERS[ALU_OUTPUT];
    }
    else{
      mips.pipeline[MEM_WB].intruction_register.dest =
          mips.pipeline[MEM_WB].SP_REGISTERS[LMD];
    }
  }
  writeBackNeeded--;
  instructionsExecuted++;
}

void processorKeyUpdate(){
  for(int i = 0; i<5; i++){
    processorKey[i] = processorKeyNext[i];
  }

}

/* body of the simulator */
void run(unsigned cycles) {
  /* If cycles has argument, run for that number of cycles
   * if not, run asm to completion*/

  /* Fetch initially needs to run, but after an EOP is recognized, fetch is
  not needed */

  //starting point for fetch reference
  //increment in fetch function
  static unsigned fetchInstruction = mips.instr_base_address;
 


  unsigned cyclesRan = 0;
  switch (cycles) {
  case NOT_DECLARED:
    //instruction_t instr = mips.instr_memory[i];
    if (mips.pipeline[EXE_MEM].intruction_register.opcode == EOP){
      break;
    }
    else {
      fetch(cyclesRan);// keep moving forward
      decode();
      execute();
    }
    break;
  default:
    while (cyclesRan < cycles) {

      /*If there is only one instruction in the pipeline, only one funciton
       * will be called. If, however, multiple instructions have been fetched, the
       * number of functions called also changes
       * I'm struggline with implementing changing the number of actions taken within the while loop*/

      //trying with this stupid array check and copy system
      processorKeyUpdate();
      if(processorKey[0]){
        fetch(fetchInstruction);
      }
      if(processorKey[4]){
        write_back();
      }
      if(processorKey[1]){
        decode();
      }
      if(processorKey[2]){
        execute();
      }
      if(processorKey[3]){
        memory();
      }
      cyclesRan++;
    }
    break;
  }
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
  return instructionsExecuted; // please modify
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
