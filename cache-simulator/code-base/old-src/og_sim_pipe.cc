#include "../include/sim_pipe.h"
//#include "cache.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>

//#define DEBUG
#define CACHE_DBG
#define PRINT_MEMORY 0

#define SHOW_STALLS 0

#define BRANCH_STALLS_NEEDED 2

#define NEW_MEM_LATENCY_STRUCTURE 1

#define NEW_RUN_STRUCTURE 1

using namespace std;

bool immediateBreak = false;
bool doDecode = true;
bool didDecode = false;
bool didLockDecode = false;
bool didMemoryStall = false;
bool doExecute = true;
bool doMemory = true;
bool doWriteBack = true;
bool doFetch = true;
bool doMemoryStall = false;
bool doBranchFetch = false;
bool doLockDecode = false;

bool structuralHazard = false;
bool dataHazard = false;

/*Branch conditional value, should only be set by conditional instructions*/
unsigned conditional_output = 0;

int processorKey[NUM_RUN_FUNCTIONS] = {1,0,0,0,0,0,0,0};
int processorKeyNext[NUM_RUN_FUNCTIONS] = {1,0,0,0,0,0,0,0};

int depStallsNeeded = 0;

int globalBranchStalls = 0;
int globalStoreDepStalls = 0;
int globalLoadDepStalls = 0;
int globalArithDepStalls = 0;
int globalLatencyStalls = 0;
int globalRAWstalls = 0;

unsigned memoryStallNumber = 0;
// used for debugging purposes
instruction_t stall_instruction = {NOP, UNDEFINED, UNDEFINED,UNDEFINED,UNDEFINED, "" };

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
inline void int2char(unsigned value, unsigned char *buffer) { //
  /* Value passed = data_memory + offset, offset determined by alu_output */
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
void sim_pipe::load_program(const char *filename, unsigned base_address) {

  /* initializing the base instruction address */
  instr_base_address = base_address;

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
    instr_memory[instruction_nr].opcode = search->second;

    // reading remaining parameters
    char *par1;
    char *par2;
    char *par3;
    switch (instr_memory[instruction_nr].opcode) {
    case ADD:
    case SUB:
    case XOR:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      par3 = strtok(NULL, " \t");
      instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
      instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
      instr_memory[instruction_nr].src2 = atoi(strtok(par3, "R"));
      break;
    case ADDI:
    case SUBI:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      par3 = strtok(NULL, " \t");
      instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
      instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
      instr_memory[instruction_nr].immediate = strtoul(par3, NULL, 0);
      break;
    case LW:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
      instr_memory[instruction_nr].immediate =
          strtoul(strtok(par2, "()"), NULL, 0);
      instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
      break;
    case SW:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
      instr_memory[instruction_nr].immediate =
          strtoul(strtok(par2, "()"), NULL, 0);
      instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
      break;
    case BEQZ:
    case BNEZ:
    case BLTZ:
    case BGTZ:
    case BLEZ:
    case BGEZ:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
      instr_memory[instruction_nr].label = par2;
      break;
    case JUMP:
      par2 = strtok(NULL, " \t");
      instr_memory[instruction_nr].label = par2;
    default:
      break;
    }

    /* increment instruction number before moving to next line */
    instruction_nr++;
  }
  // reconstructing the labels of the branch operations
  int i = 0;
  while (true) {
    instruction_t instr = instr_memory[i];
    if (instr.opcode == EOP)
      break;
    if (instr.opcode == BLTZ || instr.opcode == BNEZ || instr.opcode == BGTZ ||
        instr.opcode == BEQZ || instr.opcode == BGEZ || instr.opcode == BLEZ ||
        instr.opcode == JUMP) {
      instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
    }
    i++;
  }
  pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = instr_base_address;
}

/* writes an integer value to data memory at the specified address (use
 * little-endian format: https://en.wikipedia.org/wiki/Endianness) */
void sim_pipe::write_memory(unsigned address, unsigned value) {
  int2char(value, data_memory + address);
}

/* prints the content of the data memory within the specified address range */
void sim_pipe::print_memory(unsigned start_address, unsigned end_address) {
  cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address
       << ":0x" << hex << setw(8) << setfill('0') << end_address << "]" << endl;
  for (unsigned i = start_address; i < end_address; i++) {
    if (i % 4 == 0)
      cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
    cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
    if (i % 4 == 3)
      cout << endl;
  }
}

/* prints the values of the registers */
void sim_pipe::print_registers() {
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
sim_pipe::sim_pipe(unsigned mem_size, unsigned mem_latency) {
  data_memory_size = mem_size;
  data_memory_latency = mem_latency;
  data_memory = new unsigned char[data_memory_size];
  reset();
}

/* deallocates the pipeline simulator */
sim_pipe::~sim_pipe() { delete[] data_memory; }

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */


/////////////////////
// FETCH FUNCTIONS //
/////////////////////


void sim_pipe::fetch(){
  /*Function to get the next instruction
   * if current instruction is branch, next instruction needs to wait in decode
   * until this instruction is is ex_mem stage
   */

  /* Create variable to hold PC */
  static unsigned fetchInstruction;
  static unsigned fetchInstructionIndex;
  static unsigned valuePassedAsNPC;
  static unsigned holdBranchNPC;

  static instruction_t currentInstruction;

  static instruction_t stallInstruction={NOP,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,""};
  static bool insertStall = false;
  static int localStallCounter = 0;

  bool skip = false;
/*Determine next instruction to fetch: this is an issue b/c next instruction isn't alwasy immediately available
 * For example, if the instruction we just fetched is a branching instruction, the next fetch won't be known for two clock cycles
 * Current structure hinges on a pulling of an instruction to determine what instruction should be pulled next.*/

  /*Arith/Load/Store Instructions*/
  /*Get current PC*/
  fetchInstruction = pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC];
  /*Set NPC*/
  valuePassedAsNPC = fetchInstruction+4;
  /* set fetchInstruction to a value that can be used as an index
     * for instr_memory[] */
  fetchInstructionIndex = (fetchInstruction-0x10000000)/4;
  /*get current instruction*/
  currentInstruction = instr_memory[fetchInstructionIndex];
  /*Check if branch*/
  if(insertStall){
    /*Propogate stall*/
    pipeline.stage[IF_ID].parsedInstruction = stallInstruction;
    /*increment local stall counter*/
    localStallCounter++;
    /*increment global stall counter, to be used when returning program stall count*/
    globalBranchStalls++;
    /*check number of stall inserted*/
    if(localStallCounter==BRANCH_STALLS_NEEDED){
      /*if number of stalls inserted = number of stalls needed*/
      /*don't insert more stalls*/
      insertStall=false;
      /*reset local stall counter*/
      localStallCounter=0;
     /* if branch conditional is true */
      if(conditional_output){
        /*set instruction location for next fetch*/
        pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
      }
      else{
        pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = holdBranchNPC;
      }
      /*Dont run normal fetch function until next clock cycle*/
      skip = true;
    }
  }

  if((!insertStall)&&(!skip)){
    /*Propogate instruction*/
    pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
    /*push NPC to first pipeline register*/
    pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsNPC;
    /*Prepare value that will be used when this function is called again*/
    pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = valuePassedAsNPC;
    /*Check if current instruction requires a branch stall*/
    if (instruction_type_check(currentInstruction) == COND_INSTR) {
      /*init stall counter*/
      localStallCounter = 0;
      /*toggle stall inserter*/
      insertStall = true;
      /*save NPC */
      holdBranchNPC = valuePassedAsNPC;
    }
    /**/
    else if (currentInstruction.opcode == EOP) {
      /*EOP instructions*/
      pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = fetchInstruction;

      pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = fetchInstruction;

      pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
    }
  }
}

void sim_pipe::branch_fetch() {
  static int branchStallsInserted = 0;
  static unsigned potentialNPC;
  globalBranchStalls++;

/*Check if first insertion. If first insertion, store current NPC*/
/*If not first insertion, don't over-write the stored npc, this will be used to determine which branch to take*/
/* First time this function is called, save potential NPC */
  if (!branchStallsInserted++) {
    potentialNPC = pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
  }

  /*PASS NOP*/
  pipeline.stage[IF_ID].parsedInstruction = {NOP, UNDEFINED, UNDEFINED,
                                             UNDEFINED, UNDEFINED, ""};

  if (branchStallsInserted == BRANCH_STALLS) {
    /*check value of conditional*/
    if (instruction_type_check(pipeline.stage[EXE_MEM].parsedInstruction) ==
        COND_INSTR) {
      /*Really just an error check for debugging*/
      if (pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND]) {
        pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
      }
      else {
      /*conditon evaluated false*/
      pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = potentialNPC;
      /*now go fetch that instruction*/
      }
    }
    branchStallsInserted = 0;
    doFetch = true;
    doBranchFetch = false;
    immediateBreak=true;

  }

  else {
    doFetch=false;
  }
  /*reset counter, turn normal fetcher back on*/

}


//////////////////////
// DECODE FUNCTIONS //
//////////////////////


void sim_pipe::decode()
{
  /*Function to parse the register file into special purpose registers
   */
  /*forward instruction register from ID_EXE stage*/
  /*Create variable to hold current instruction*/
  static instruction_t currentInstruction;
  /*Create variable to hold stall instruction*/
  static unsigned lastCheckedInstruction = UNDEFINED;
  static bool dependencyLock = false;
  static unsigned depStallsNeeded = 0;
  static unsigned depStallsInserted = 0;
  /*If the pipeline isn't already locked, chech for dependency*/
  /*RESTRUCTURING: no need to check for dep lock, because */
  if(dependencyLock){
    if(depStallsInserted != depStallsNeeded){
      insert_stall(ID_EXE);
      depStallsInserted++;
    }
    else {
      normal_decode(currentInstruction);
      pipeline.stage[ID_EXE].parsedInstruction = currentInstruction;
      globalRAWstalls += depStallsInserted;
      depStallsInserted = 0;
      dataHazard = false;
      dependencyLock = false;
        }
  }
  /*If function is not currently in locked state*/
  else{
    currentInstruction = pipeline.stage[IF_ID].parsedInstruction;
    if (currentInstruction.opcode != NOP && currentInstruction.opcode != EOP &&
        lastCheckedInstruction != currentInstruction.opcode) {
      depStallsNeeded = data_dep_check(currentInstruction);
    }
    else {
      depStallsNeeded = 0;
    }
    if (!depStallsNeeded) {
      /*create function to handle normal decode*/
      normal_decode(currentInstruction);
      lastCheckedInstruction = UNDEFINED;
    }
    else if(depStallsNeeded){
      lastCheckedInstruction = currentInstruction.opcode;
      insert_stall(ID_EXE);
      depStallsInserted++;
      dependencyLock = true;
      dataHazard = true;
    }
    if (currentInstruction.opcode == EOP) {
      doDecode = false;
      doLockDecode = false;
    }
  }
}

void sim_pipe::normal_decode(instruction_t currentInstruction) {
  
  pipeline.stage[ID_EXE].parsedInstruction = currentInstruction;
  /*different functions pass different values through the sp registers. This
   * is the start of conditional logic to determine which values should be
   * pushed through, but it is incomplete*/
  switch (currentInstruction.opcode) {
  case SW:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] =
        gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] =
        gp_registers[currentInstruction.src2];
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] =
        currentInstruction.immediate;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  case LW:
    /*don't pass A*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] =
        gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] =
        currentInstruction.immediate;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  case ADD:
  case SUB:
  case XOR:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] =
        gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] =
        gp_registers[currentInstruction.src2];
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = UNDEFINED;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  case ADDI:
  case SUBI:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] =
        gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] =
        currentInstruction.immediate;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  case BEQZ:
  case BGEZ:
  case BGTZ:
  case BLEZ:
  case BLTZ:
  case BNEZ:
  case JUMP:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] =
        currentInstruction.immediate;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  default:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = UNDEFINED;
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = UNDEFINED;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  }
  // decrement number of decodes stages needed
}

void sim_pipe::lock_decode() {
  /*Pass NOP instruction*/
  static instruction_t stalled_instruction;
  static int depStallsInserted = 0;
  pipeline.stage[ID_EXE].parsedInstruction = {NOP, UNDEFINED, UNDEFINED,
                                              UNDEFINED, UNDEFINED, ""};
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = UNDEFINED;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = UNDEFINED;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] = UNDEFINED;

  /*check to see if more stalls will be needed*/
  doExecute = true;
  if(!depStallsInserted++){
    stalled_instruction=pipeline.stage[IF_ID].parsedInstruction;
    /*Turn fetcher off*/
    doFetch=false;
    /*Next clock cycle, return to lock_decode()*/
    doLockDecode=true;
    /*Next clock cycle, do not run normal decode*/
    doDecode=false;
  }
  if(depStallsInserted!=depStallsNeeded){
    processorKey[ID_R] = 0;
  }
  /*If the appropriate number of stalls has been inserted*/
  if (depStallsInserted==depStallsNeeded) {
    if(depStallsNeeded==1){
      immediateBreak = true;
    }
    //normal_decode(stalled_instruction);
    doLockDecode=false;
    /*Turn normal decoder on (for next clock cycle)*/
    doDecode=true;
    /*Turn fetcher On (for this clock cycle)*/
    //processorKey[0] = 1;
    /*Turn fetcher On (for next clock cycle)*/
    doFetch = true;
    depStallsInserted = 0;
    depStallsNeeded = 0;
  }
  immediateBreak = true;
  didLockDecode = true;
}

/*Function to check if flow dependencies exist in the pipeline, return number of stalls needed*/
int sim_pipe::data_dep_check(instruction_t checkedInstruction) {
  /*array to hold instructions that exist further down the pipeline. I believe
   * that the only pipeline registers I'm concerned with are the decode/execute
   * register and execute/memory register. The memory/writeback register will
   * already have been processed by the time this check happens*/
  static int PRE_MEMORY = 0;
  static int PRE_WRITE_BACK = 1;
  static int FORWARD_STAGES = 2;
  int retVal = 0;

  instruction_t pipelineInstructions[FORWARD_STAGES];

  pipelineInstructions[PRE_MEMORY] = pipeline.stage[EXE_MEM].parsedInstruction;
  pipelineInstructions[PRE_WRITE_BACK] =
      pipeline.stage[MEM_WB].parsedInstruction;

  for (int i = 0; i < FORWARD_STAGES; i++) {
    if (single_source_check(checkedInstruction)) {
      if (pipelineInstructions[i].dest == checkedInstruction.src1) {
        if (i == PRE_MEMORY) {
          retVal=stage_location(pipelineInstructions[i].opcode);
          globalLoadDepStalls+=retVal;
          //globalRAWstalls+=retVal;
          //stalls+=retVal;
          return retVal;
        }
        else{
          retVal=stage_location(pipelineInstructions[i].opcode)-1;
          globalStoreDepStalls+=retVal;
          //globalRAWstalls+=retVal;
          //stalls++;
          return retVal;
        }
      }
    } else {
      if (pipelineInstructions[i].dest == checkedInstruction.src1 || pipelineInstructions[i].dest == checkedInstruction.src2){
        if(i==PRE_MEMORY){
          globalLoadDepStalls+=retVal;
          retVal=stage_location(pipelineInstructions[i].opcode);
          //globalRAWstalls+=retVal;
          //stalls+=retVal;
          return retVal;
        }
        else{
          retVal=stage_location(pipelineInstructions[i].opcode)-1;
          globalStoreDepStalls+=retVal;
          //globalRAWstalls+=retVal;
          //stalls++;
          return retVal;
        }
      }
    }
  }
  return 0;
}
/* if either of the following pipeline register's destination (write back
 * location) contain either argument found in the current instruction, a
 * flow hazard exists */

int sim_pipe::stage_location(opcode_t checkOpcode) {
  static int forwardStages = 2;
  switch (checkOpcode) {
  case ADD:
  case ADDI:
  case SUB:
  case SUBI:
  case XOR:
    return forwardStages;
    break;
  case LW:
    //return forwardStages + data_memory_latency;
    // Trying this
    return forwardStages;
    break;
  case SW:
    // return forwardStages + data_memory_latency - 1;
    // trying this
    return forwardStages - 1;
  default:
    return 0;
  }
  return 0;
}


bool sim_pipe::single_source_check(instruction_t checkInstruction) {
  if (checkInstruction.opcode == ADDI || checkInstruction.opcode == SUBI ||
      checkInstruction.opcode == LW || checkInstruction.opcode == SW) {
    return true;

  } else {
    return false;
  }
}


///////////////////////
// EXECUTE FUNCTIONS //
///////////////////////


void sim_pipe::execute() {
  /*ID_EXE -> EXE_MEM*/
  /* Forward instruction register from  */
  pipeline.stage[EXE_MEM].parsedInstruction =
      pipeline.stage[ID_EXE].parsedInstruction;
  instruction_t currentInstruction = pipeline.stage[ID_EXE].parsedInstruction;
  /* get A to use in this stage */
  unsigned executeA = pipeline.stage[ID_EXE].spRegisters[ID_EXE_A];
  /* get B to use in this stage */
  unsigned executeB = pipeline.stage[ID_EXE].spRegisters[ID_EXE_B];
  /* Forward B*/
  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B] = executeB;

  /* get imm to use in this stage */
  unsigned executeImm= pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM];
  /* get NPC to use in this stage */
  unsigned executeNPC= pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC];

  kind_of_instruction_t currentInstructionType = instruction_type_check(currentInstruction);
  /*Generate conditional output*/
  if(currentInstructionType == COND_INSTR){
    /*If this instruction is a conditional*/
    /*set global variable here, to be used by fetch instruction*/
    conditional_output = conditional_evaluation(executeA, currentInstruction.opcode);
  }
  else{
    conditional_output = 0;
  }

  /*pass cond output value to sp registers*/
  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND] = conditional_output;


  /*take ALU action*/
  unsigned currentALUOutput = alu(currentInstruction.opcode,executeA,executeB,executeImm,executeNPC);
  /*pass alu output*/
  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT] = currentALUOutput;
  // decrement number of execute stages needed
  // increment number of memory stages needed
  if(currentInstruction.opcode==EOP){
    doExecute=false;
  }
}

unsigned sim_pipe::conditional_evaluation(unsigned evaluate, opcode_t condition){
  switch (condition){
  case BEQZ:
    return (evaluate == 0);
  case BNEZ:
    return(evaluate!=0);
  case BGTZ:
    return(evaluate>0);
  case BGEZ:
    return(evaluate>=0);
  case BLTZ:
    return(evaluate<0);
  case BLEZ:
    return(evaluate<=0);
  default:
    return 0;
  }
}


//////////////////////
// MEMORY FUNCTIONS //
//////////////////////

void sim_pipe::memory() {
  // propogate IR
  static bool latency_hold = false;
  static instruction_t currentInstruction;
  static opcode_t currentOpcode;
  static unsigned currentALUOutput;
  static unsigned latency_stalls_inserted = 0;
  static unsigned currentB;

  if(latency_hold){
    //upon entry to this function, a stall has already been inserted
    //I think it should just be 'data_mem_latency' or whatever the var is as counter
    //if more stalls needed
    if(latency_stalls_inserted<data_memory_latency){
      // insert more stalls
      latency_hold = true;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
      pipeline.stage[MEM_WB].parsedInstruction = {NOP};
      //increment stall counter
      latency_stalls_inserted++;
    }
    else{
      //if approp number of stalls inserted, turn staller off
      structuralHazard = false;
      latency_hold = false;
      //increment global counter
      globalLatencyStalls+=latency_stalls_inserted;
      //reset latency counter
      latency_stalls_inserted = 0;

      pipeline.stage[MEM_WB].parsedInstruction = currentInstruction;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;

      if (currentOpcode == SW) {
        memory_store(currentB, currentALUOutput);
      }
      if(currentOpcode == LW){
        memory_load(currentALUOutput);
      }
    }

  }

  else if(!latency_hold){
    //get current instruction
    currentInstruction = pipeline.stage[EXE_MEM].parsedInstruction;
    //get current opcode
    currentOpcode = currentInstruction.opcode;
    //get ALU output
    currentALUOutput = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
    //if this is a memory dependent instruction, pass a NOP, hold info
    currentB = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
    if((instruction_type_check(currentInstruction)==LWSW_INSTR)&&(data_memory_latency>latency_stalls_inserted)){
      latency_hold = true;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
      pipeline.stage[MEM_WB].parsedInstruction = {NOP};
      structuralHazard = true;
      latency_stalls_inserted++;
      //decode();
      //if(!dataHazard)
      //fetch();
    }
    else{
      pipeline.stage[MEM_WB].parsedInstruction = currentInstruction;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD]= UNDEFINED;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;
      if (currentInstruction.opcode == SW) {
        memory_store(currentB, currentALUOutput);
      } else if (currentInstruction.opcode == LW) {
        memory_load(currentALUOutput);
      }
    }
  }

#if !NEW_MEM_LATENCY_STRUCTURE
  switch (currentOpcode) {
  case LW:
  case SW:
    memory_stall();
    break;
  case EOP:
    doMemory=false;
    break;
  default:
    doWriteBack=true;
    break;
  }
#endif
}

void sim_pipe::memory_stall(){
  static unsigned stalledB;
  static instruction_t stalledInstruction;
  static unsigned stalledALUOutput;
  static unsigned char* whatToLoad;
  static unsigned loadableData;

  if(!memoryStallNumber++){
   stalledB =  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
   stalledInstruction = pipeline.stage[EXE_MEM].parsedInstruction;
   stalledALUOutput = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
  }

  if(memoryStallNumber<(data_memory_latency+1)) {
    /*insert another stall*/
    /*Keep other units off*/
    doFetch=false;
    doDecode=false;
    doExecute=false;
    doMemory=false;
    doMemoryStall=true;
    globalLatencyStalls++;
    pipeline.stage[MEM_WB].parsedInstruction = {NOP};
  } else {
    doFetch=true;
    doDecode=true;
    doExecute=true;
    doMemory=true;
    doMemoryStall=false;
    /*reset memory stall counter*/
    memoryStallNumber = 0;
    switch (stalledInstruction.opcode) {
    case LW:
      /*Determine load value by referencing the data_memory array, at index
       * generated by the ALU*/
      whatToLoad = &data_memory[stalledALUOutput];
      #if PRINT_MEMORY
      print_memory(0,32);
      #endif
      loadableData = char2int(whatToLoad);
#if PRINT_MEMORY
      cout << "Preparing to load: " << loadableData << " from memory" << endl;
#endif
      /*Pass the reference of the load value to conversion function
       * Pass conversion function ot next stage of pipeline*/
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = loadableData;
      break;
    case SW:
      break;
      default:
        break;
    }
    pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = stalledALUOutput;
    processorKeyNext[WB_R]++;
  }
}

void sim_pipe::memory_store(unsigned thisB, unsigned thisALUOutput){
      /*Pass memory address (pulled from register, processed with ALU) to
       * conversion function THIS conversion function handles writing to memory
       * (store-word doesn't need a write_back() call)
       */
#if PRINT_MEMORY
      cout<<"Inserting: "<<stalledB<<" at "<< stalledALUOutput<< " offset from base addr." << endl;
#endif
      int2char(thisB, data_memory + thisALUOutput);
#if PRINT_MEMORY
      print_memory(0,32);
#endif
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;

}

void sim_pipe::memory_load(unsigned thisALUOutput){
  static unsigned char* whatToLoad;
  static unsigned loadableData;
  whatToLoad = &data_memory[thisALUOutput];
#if PRINT_MEMORY
  print_memory(0,32);
#endif
  loadableData = char2int(whatToLoad);
#if PRINT_MEMORY
  cout << "Preparing to load: " << loadableData << " from memory" << endl;
#endif
  /*Pass the reference of the load value to conversion function
   * Pass conversion function ot next stage of pipeline*/
  pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = loadableData;


}

//////////////////////////
// WRITE BACK FUNCTIONS //
//////////////////////////

void sim_pipe::write_back() {
  //put whats in the alu output register into the destination that
  //is in the destination register
  //if load instruction, pass LMD to register
  //if arithmatic, pass alu output
  //either store word or load word happens in the memory stage, not sure which one
  instruction_t currentInstruction = pipeline.stage[MEM_WB].parsedInstruction;
  opcode_t currentOpcode = currentInstruction.opcode;
  unsigned currentALUOutput = pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT];
  unsigned currentLMD = pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD];
  bool writeBackNeeded;
  switch(currentOpcode){
    case LW:
    case ADD:
    case SUB:
    case ADDI:
    case SUBI:
    case XOR:
      writeBackNeeded = true;
      break;
    default:
      writeBackNeeded = false;
      break;
  }
  if (writeBackNeeded) {
    int registerIndex = currentInstruction.dest;
    switch(currentOpcode){
      case LW:
        gp_registers[registerIndex] = currentLMD;
        pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
        break;
      default:
        gp_registers[registerIndex] = currentALUOutput;
        pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
        break;
    }
  }
  if(currentInstruction.opcode == EOP){
    program_complete=true;
    pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
  }
  if(currentInstruction.opcode != NOP && currentInstruction.opcode != EOP)
  {
    instructions_executed++;
  }
}


///////////////////////
// CONTROL FUNCTIONS //
///////////////////////


/* body of the simulator */
void sim_pipe::run(unsigned cycles) {
  switch (cycles) {
  case CYCLES_NOT_DECLARED:
    while (!program_complete) {
      run_clock();
      if(!program_complete)
      clock_cycles++;
    }
    break;
  default:
    unsigned cyclesThisRun = 0;
    while (cyclesThisRun < cycles) {
      run_clock();
      cyclesThisRun++;
      clock_cycles++;
    }
    break;
  }
}

void sim_pipe::run_clock() {
  immediateBreak = false;
  didLockDecode = false;
  didMemoryStall = false;
  processor_key_update();
#if NEW_RUN_STRUCTURE
  write_back();
  memory();
  if (!structuralHazard) {
    execute();
    decode();
    if (!dataHazard) {
      fetch();
    }
  }
#else
      if (doWriteBack) {
        write_back();
      }
      if(doMemoryStall){
        memory_stall();
        didMemoryStall=true;
      }
      if (!didMemoryStall&&doMemory) {
        memory();
      }
      if (doExecute) {
        execute();
      }
      if (doLockDecode) {
        lock_decode();
        didLockDecode=true;
        //clock_cycles++;

      }
      if (!didLockDecode&&doDecode) {
        decode();
      }
      if (!immediateBreak) {
        fetch();
      }
 #endif
}

/* reset the state of the pipeline simulator */
void sim_pipe::reset() {
    /* Clear gp registers */
    for(int i = 0; i<NUM_GP_REGISTERS; i++){
        gp_registers[i] = UNDEFINED;
    }
    /* Clear data memory */
    for(unsigned i = 0; i<data_memory_size;i++){
        data_memory[i] = 0xFF;
    }
    /* Clear sp registers */
    for(int i = 1; i<NUM_STAGES;i++){
        for(int j = 0; j< NUM_SP_REGISTERS; j++){
          pipeline.stage[i].spRegisters[j] = UNDEFINED;
          pipeline.stage[i].parsedInstruction = {NOP,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,""};
        }
    }

}


//////////////////////
// HELPER FUNCTIONS //
//////////////////////

// return value of special purpose register
unsigned sim_pipe::get_sp_register(sp_register_t reg, stage_t s) {
  /* pipeline object has 4 stages, processor has 5 stages. Return sp register of
   * pipeline object preceding the stage argument*/
  if(s == IF){
    switch(reg){
      case PC:
        return pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC];
       break;
      default:
        return UNDEFINED;
        break;
    }
  }
  if(s == ID){
    switch(reg){
      case NPC:
        return pipeline.stage[s].spRegisters[IF_ID_NPC];
       break;
      default:
        return UNDEFINED;
        break;
    }
  }
  if(s == EXE){
    switch(reg){
      case A:
        return pipeline.stage[s].spRegisters[ID_EXE_A];
        break;
      case B:
        return pipeline.stage[s].spRegisters[ID_EXE_B];
        break;
      case IMM:
        return pipeline.stage[s].spRegisters[ID_EXE_IMM];
        break;
      case NPC:
        return pipeline.stage[s].spRegisters[ID_EXE_NPC];
        break;
      default:
        return UNDEFINED;
        break;
      }
  }
  if(s==MEM){
    switch(reg){
      case B:
        return pipeline.stage[s].spRegisters[EXE_MEM_B];
        break;
      case ALU_OUTPUT:
        return pipeline.stage[s].spRegisters[EXE_MEM_ALU_OUT];
        break;
      default:
        return UNDEFINED;
        break;
    }
  }
  if(s==WB){
    switch(reg){
      case ALU_OUTPUT:
        return pipeline.stage[s].spRegisters[MEM_WB_ALU_OUT];
        break;
      case LMD:
        return pipeline.stage[s].spRegisters[MEM_WB_LMD];
        break;
      default:
        return UNDEFINED;
        break;
    }
  }
  else{
    return 0xff;
  }
}

// returns value of general purpose register
int sim_pipe::get_gp_register(unsigned reg) {
  return gp_registers[reg]; // please modify
}

void sim_pipe::set_gp_register(unsigned reg, int value) {
    gp_registers[reg] = value;
}

float sim_pipe::get_IPC() {
  return instructions_executed/clock_cycles; // please modify
}

unsigned sim_pipe::get_instructions_executed() {
  return instructions_executed; // please modify
}

unsigned sim_pipe::get_stalls() {
#if SHOW_STALLS
  cout << "Number of branch stalls: " << globalBranchStalls<< endl;
  cout << "Number of data dependency stalls: " << globalRAWstalls<< endl;
  cout << "Number of memory latency stalls: " << globalLatencyStalls << endl;
#endif
  stalls = globalBranchStalls + globalRAWstalls + globalLatencyStalls;
  return stalls; // please modify
}

unsigned sim_pipe::get_clock_cycles() {
  return clock_cycles; // please modify
}

void sim_pipe::set_sp_reg(pipeline_stage_t thisStage, int reg, unsigned registerVal){
  pipeline.stage[thisStage].spRegisters[reg]=registerVal;
}

instruction_t sim_pipe::get_sp_reg_instruction(pipeline_stage_t thisStage,
                                               instruction_t thisInstruction) {
  return pipeline.stage[thisStage].parsedInstruction;
}

void sim_pipe::set_sp_reg_instruction(pipeline_stage_t thisStage,
                                      instruction_t thisInstruction) {
  pipeline.stage[thisStage].parsedInstruction = thisInstruction;
}

void sim_pipe::processor_key_update(){
  for (int i = 0; i < NUM_RUN_FUNCTIONS; i++) {
    int &keyVal = processorKeyNext[i];
    processorKey[i] = keyVal;
  }
}

void sim_pipe::set_program_complete(){
  program_complete = true;
}


bool sim_pipe::get_program_complete(){
  return program_complete;
}

void sim_pipe::insert_stall(pipeline_stage_t nextStage){
  /*set parsed instruction to NOP*/
  pipeline.stage[nextStage].parsedInstruction = stall_instruction;
  /*set all sp registers to UNDEFINED */
  for(unsigned i = 0; i<NUM_SP_REGISTERS; i++){
    pipeline.stage[nextStage].spRegisters[i] = UNDEFINED;
  }

}

kind_of_instruction_t
sim_pipe::instruction_type_check(instruction_t checkedInstruction) {
  switch (checkedInstruction.opcode) {
  case ADD:
  case ADDI:
  case SUB:
  case SUBI:
  case XOR:
    return ARITH_INSTR;
  case LW:
  case SW:
    return LWSW_INSTR;
  case BEQZ:
  case BNEZ:
  case BGTZ:
  case BGEZ:
  case BLTZ:
  case BLEZ:
  case JUMP:
    return COND_INSTR;
  case EOP:
  case NOP:
    return NOPEOP_INSTR;
  }
  return NOPEOP_INSTR;
}

void sim_pipe::set_cache(cache *c){
  memory_cache = c;
#ifdef CACHE_DBG
  memory_cache->print_configuration();
#endif
}
