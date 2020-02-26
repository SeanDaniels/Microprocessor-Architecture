#include "sim_pipe.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>

//#define DEBUG

using namespace std;

int decodeNeeded = 0;
int executeNeeded = 0;
int memoryNeeded = 0;
int writeBackNeeded = 0;
int processorKey[NUM_RUN_FUNCTIONS] = {1,0,0,0,0,0,0,0};
int processorKeyNext[NUM_RUN_FUNCTIONS] = {1,0,0,0,0,0,0,0};
int depStallsNeeded = 0;
int memoryStallNumber = 0;
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
void sim_pipe::processor_key_update(){
  for (int i = 0; i < NUM_RUN_FUNCTIONS; i++) {
    processorKey[i] = processorKeyNext[i];
  }
}

void sim_pipe::set_program_complete(){
  program_complete = true;
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

bool sim_pipe::get_program_complete(){
  return program_complete;
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
}

void sim_pipe::branch_fetch() {
  /* Need some way to control this better. This setup would call this over and
   * over for any given branch */
  /*Set counter for stalls added*/
  static int branchStallsInserted = 0;
  static unsigned potentialNPC;
  static unsigned immediate;
/*Check if first insertion. If first insertion, store current NPC*/
/*If not first insertion, don't over-write the stored npc, this will be used to determine which branch to take*/

  if (!branchStallsInserted) {
    potentialNPC = pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
  }

  /*PASS NOP*/
  pipeline.stage[IF_ID].parsedInstruction = {NOP, UNDEFINED, UNDEFINED,
                                             UNDEFINED, UNDEFINED, ""};
  /*forward NOP NPC*/
  /*If this is first insertion call*/
  /*increment insertion counter*/
  branchStallsInserted++;
  stalls=branchStallsInserted;
  /*If more branchStallsInserted are needed*/
  if (branchStallsInserted != BRANCH_STALLS) {
    processorKeyNext[IF_R] = 0;
    processorKey[IF_R] = 0;
    processorKeyNext[B_IF_R] = 1;
  }
  /*If the required amount of insertions*/
  if (branchStallsInserted == BRANCH_STALLS) {
    /*check value of conditional*/
    if (instruction_type_check(pipeline.stage[EXE_MEM].parsedInstruction) ==
        COND_INSTR) {
      /*Really just an error check for debugging*/
      if (pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND]) {
        /*If condition evaluated true*/
        /*get label*/
        immediate = pipeline.stage[EXE_MEM].parsedInstruction.immediate;
        immediate = (immediate + 4 - 0xFFFFFFE0) / 4;
        immediate = (immediate * 4) + 0x10000000;
        pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = immediate;
      }
      else {
      /*conditon evaluated false*/
      pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = potentialNPC;
      /*now go fetch that instruction*/
      }
    } 
  branchStallsInserted = 0;
  processorKey[IF_R] = 1;
  processorKeyNext[IF_R] = 1;
  processorKeyNext[B_IF_R] = 0;
  }
  /*reset counter, turn normal fetcher back on*/
 

  processorKeyNext[ID_R]++;
}

void sim_pipe::fetch(){
  /*Function to get the next instruction
   * if current instruction is branch, next instruction needs to wait in decode
   * until this instruction is is ex_mem stage
   */

  /* Create variable to hold PC */
  static unsigned fetchInstruction;
  static unsigned fetchInstructionIndex;
  static unsigned valuePassedAsPC;
  static instruction_t currentInstruction;
  /*IDK if the following declaration will work */
  static instruction_t stallInstruction={NOP};
  static int stallsNeeded = 0;
  static int potentialNPC;
/*Determine next instruction to fetch: this is an issue b/c next instruction isn't alwasy immediately available
 * For example, if the instruction we just fetched is a branching instruction, the next fetch won't be known for two clock cycles
 * Current structure hinges on a pulling of an instruction to determine what instruction should be pulled next.*/

/*Arith/Load/Store Instructions*/
  /*Get current PC*/
  fetchInstruction = pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC];
  /*Set NPC*/
  valuePassedAsPC = fetchInstruction+4;
  /* set fetchInstruction to a value that can be used as an index
     * for instr_memory[] */
  fetchInstructionIndex = (fetchInstruction-0x10000000)/4;
  /*get current instruction*/
  currentInstruction = instr_memory[fetchInstructionIndex];
  /*Check if branch*/
  if(instruction_type_check(currentInstruction)==COND_INSTR){
    /*If current instruction is a branching one, turn fetch off, let some other function handle this*/
    /* Still want to pass this current instruction forward */
    pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
    /*push NPC to first pipeline register*/
    pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;
    /*Maybe update Pipeline PC here, so its ready when this fetcher gets turned back on?*/
    /*branch_fetch will run on next clock cycle, inserting a NOP*/
    //branch_fetch();
    /*This stage runs last, only set next values, processor_key_update will fix the rest of the values*/
    processorKeyNext[IF_R] = 0;
    processorKeyNext[B_IF_R] = 1;
  }
  /*Arith/Load/Store instruction*/
  else if(currentInstruction.opcode!=EOP){
    pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
    /*push NPC to first pipeline register*/
    pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;
    /*Set PC for next fetch*/
    pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = valuePassedAsPC;
  }
  /*Check if EOP*/
  else{
    /*EOP instructions*/
    /*Set pc to undefined??*/
    pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = fetchInstruction;
    /*Set npc to undefined??*/
    pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] =
        pipeline.stage[IF_ID].spRegisters[PIPELINE_PC];
    /*Set fetcher to zero*/
    pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
    processorKeyNext[IF_R] = 0;
    //pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;
  }
  /*push instruction to first pipeline register forward*/
    processorKeyNext[ID_R]++;

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

  instruction_t pipelineInstructions[FORWARD_STAGES];
  /*Need 2 stalls*/
  pipelineInstructions[PRE_MEMORY] = pipeline.stage[EXE_MEM].parsedInstruction;
  /*if function is in mem WB already, next time the proc runs, decode will be
   * ready*/
  /*Need one stall*/
  pipelineInstructions[PRE_WRITE_BACK] =
      pipeline.stage[MEM_WB].parsedInstruction;
  /* Loop through entries in pipeline instruction array
   */
  /*get memory latency*/

  for (int i = 0; i < FORWARD_STAGES; i++) {
    /* if either of the following pipeline register's destination (write back
     * location) contain either argument found in the current instruction, a
     * flow hazard exists */
    if (pipelineInstructions[i].dest == checkedInstruction.src1 ||
        pipelineInstructions[i].dest == checkedInstruction.src2) {
      /*return integer value of stage that the pending instruction is in*/
      /* should go ahead and just return the number of stalls needed here */
      /* i = 0 would mean that the pending instruction is in id/exe stage,
       * meaning 2 stall are necessary */
      /* i = 1 would mean that the pending instruction is in exe/mem stage,
       * meaning 1 stall is necessary */
      /*Normall two stalls, but a memlatency of 4 would require 5 stalls*/
      if (i == PRE_MEMORY) {
        switch (pipelineInstructions[PRE_MEMORY].opcode) {
        case ADD:
        case ADDI:
        case SUB:
        case SUBI:
        case XOR:
          return FORWARD_STAGES;
          break;
        case LW:
          return FORWARD_STAGES + data_memory_latency;
          break;
        case SW:
          return FORWARD_STAGES + data_memory_latency - 1;
        default:
          return 0;
        }
      }

      else {
        switch (pipelineInstructions[PRE_WRITE_BACK].opcode) {
        case ADD:
        case ADDI:
        case SUB:
        case SUBI:
        case XOR:
          return FORWARD_STAGES - 1;
          break;
        case LW:
          return FORWARD_STAGES - 1 + data_memory_latency;
          break;
        case SW:
          return FORWARD_STAGES + data_memory_latency - 2;
        default:
          return 0;
        }
      }
    }
  }
  return 0;
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
  processorKeyNext[ID_R]--;
  // increment number of execute stages needed
  processorKeyNext[EXE_R]++;
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
  processorKeyNext[EXE_R]++;
  if(!depStallsInserted){
    stalled_instruction=pipeline.stage[IF_ID].parsedInstruction;
  }
  if (++depStallsInserted!=depStallsNeeded) {
    depStallsInserted++;
    processorKeyNext[L_ID_R] = 1;
  }
  else{
    /*Turn staller off (for next clock cycle)*/
    stalls+=depStallsInserted;
    normal_decode(stalled_instruction);
    processorKeyNext[L_ID_R] = 0;
    /*Turn normal decoder on (for next clock cycle)*/
    processorKeyNext[ID_R] = 1;
    /*Turn fetcher On (for this clock cycle)*/
    //processorKey[0] = 1;
    /*Turn fetcher On (for next clock cycle)*/
    processorKeyNext[IF_R] = 1;
    processorKey[IF_R] = 1;
    depStallsInserted = 0;
    depStallsNeeded = 0;
  }
}

void sim_pipe::decode()
{
  /*Function to parse the register file into special purpose registers
   */
  /*forward instruction register from ID_EXE stage*/
  /*Create variable to hold current instruction*/
  instruction_t currentInstruction =  pipeline.stage[IF_ID].parsedInstruction;
  /*Create variable to hold stall instruction*/
  static instruction_t stallInstruction = {NOP,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,""};
  /*If the pipeline isn't already locked, chech for dependency*/
  /*RESTRUCTURING: no need to check for dep lock, because */
  if(currentInstruction.opcode!=NOP && currentInstruction.opcode!=EOP){
      depStallsNeeded = data_dep_check(currentInstruction);
  }
  else
  {
    depStallsNeeded=0;
  }
  

  if (!depStallsNeeded) {
    /*create function to handle normal decode*/
    normal_decode(currentInstruction);
  } else {
    /*handle stall decode*/
    /*turn fetcher off (for this clock cycle and next clock cycle)*/
    processorKey[IF_R] = 0;
    processorKeyNext[IF_R] = 0;
    /*turn this decoder off*/
    processorKeyNext[ID_R] = 0;
    /*Send stall*/
    /*after calling lock decode*/
    lock_decode();
    /*Normally, fetcher controls decoder. Circumvent that by toggling decoder on
     * here*/
    /* Decoder is only decremented in normal decode stage */
    // processorKeyNext[ID]++;

  }
}

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


  /* Generate conditional output */
  /* But don't overwrite current conditional output if its needed */
  if (instruction_type_check(currentInstruction) != NOPEOP_INSTR) {
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND] = conditional_evaluation(
        pipeline.stage[ID_EXE].spRegisters[ID_EXE_A],
        pipeline.stage[EXE_MEM].parsedInstruction.opcode);
  }
  else{
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND] = UNDEFINED;
  }

  /*take ALU action*/
  unsigned currentALUOutput = alu(currentInstruction.opcode,executeA,executeB,executeImm,executeNPC);
  
  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT] = currentALUOutput;
  // decrement number of execute stages needed
  processorKeyNext[EXE_R]--;
  // increment number of memory stages needed
  processorKeyNext[MEM_R]++;
}

void sim_pipe::memory_stall(){
  static unsigned stalledB;
  static instruction_t stalledInstruction;
  static unsigned stalledALUOutput;
  static unsigned char* whatToLoad;
  static unsigned whereToLoadFrom;
  static unsigned loadableData;

  if(!memoryStallNumber){
   stalledB =  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
   stalledInstruction = pipeline.stage[EXE_MEM].parsedInstruction;
   stalledALUOutput = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
  }

  if(memoryStallNumber++!=data_memory_latency) {
    /*insert another stall*/
    /*Keep other units off*/
    processorKey[IF_R] = 0;
    processorKey[ID_R] = 0;
    processorKey[EXE_R] = 0;
    processorKey[MEM_R] = 0;
    processorKeyNext[IF_R] = 0;
    processorKeyNext[ID_R] = 0;
    processorKeyNext[EXE_R] = 0;
    processorKeyNext[MEM_R] = 0;
    processorKeyNext[S_MEM_R] = 1;

    stalls++;
    // pipeline.stage[EXE_MEM].parsedInstruction = {NOP};
  } else {
    processorKey[IF_R] = 1;
    processorKeyNext[IF_R] = 1;
    processorKey[ID_R] = 1;
    processorKeyNext[ID_R] = 1;
    processorKey[EXE_R] = 1;
    processorKeyNext[EXE_R] = 1;
    processorKeyNext[MEM_R] = 1;
    processorKeyNext[S_MEM_R] = 0;

    /*reset memory stall counter*/
    memoryStallNumber = 0;
    switch (stalledInstruction.opcode) {
    case LW:
      /*Determine load value by referencing the data_memory array, at index
       * generated by the ALU*/
      whatToLoad = &data_memory[stalledALUOutput];
      /*Pass the reference of the load value to conversion function
       * Pass conversion function ot next stage of pipeline*/
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = char2int(whatToLoad);
      break;
    case SW:
      /*Pass memory address (pulled from register, processed with ALU) to
       * conversion function THIS conversion function handles writing to memory
       * (store-word doesn't need a write_back() call)
       */
      int2char(stalledB, data_memory + stalledALUOutput);
      break;
    default:
      break;
    }
    pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = stalledALUOutput;
    processorKeyNext[WB_R]++;
  }
}

void sim_pipe::memory() {
  unsigned char* whatToLoad;
  unsigned whereToLoadFrom;
  unsigned loadableData;
  //get B
  unsigned currentB = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
  // propogate IR
  instruction_t currentInstruction =
      pipeline.stage[EXE_MEM].parsedInstruction;
  opcode_t currentOpcode = currentInstruction.opcode;
  pipeline.stage[MEM_WB].parsedInstruction = currentInstruction;

  //
  /*paropogate ALU output*/
  /*only if instruction is not LW or SW*/
  unsigned currentALUOutput =
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];

switch(currentOpcode){
    case LW:
    case SW:
      memory_stall();
      /* Write to register ahead? */
      //pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;
      break;
    default:
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;
      processorKeyNext[WB_R]++;
      break;
  }
  // decrement number of memory stages needed
  processorKeyNext[MEM_R]--;
  // Conditionally increment number of write back stages needed
}

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
  }
  if(currentInstruction.opcode != NOP && currentInstruction.opcode != EOP)
  {
    instructions_executed++;
  }
  processorKeyNext[WB_R]--;
}
/* body of the simulator */
void sim_pipe::run(unsigned cycles) {
  switch (cycles) {
  case CYCLES_NOT_DECLARED:
    while (!program_complete) {
      processor_key_update();
      if (processorKey[WB_R]) {
        write_back();
      }
      if (processorKey[MEM_R]) {
        memory();
      }
      if(processorKey[S_MEM_R]){
        memory_stall();
      }
      if (processorKey[EXE_R]) {
        execute();
      }
      if (processorKey[L_ID_R]) {
        lock_decode();
      }
      if (processorKey[ID_R]) {
        decode();
      }
      if (processorKey[B_IF_R]) {
        branch_fetch();
      }
      if (processorKey[IF_R]) {
        fetch();
      }
      clock_cycles++;
    }
    clock_cycles--;
      break;
  default:
    unsigned cyclesThisRun = 0;
    while (cyclesThisRun < cycles) {
      /*If there is only one instruction in the pipeline, only one funciton
       * will be called. If, however, multiple instructions have been fetched,
       * the number of functions called also changes*/
      processor_key_update();
      if (processorKey[WB_R]) {
        write_back();
      }
      if (processorKey[MEM_R]) {
        memory();
      }
      if(processorKey[S_MEM_R]){
        memory_stall();
      }
      if (processorKey[EXE_R]) {
        execute();
      }
      if (processorKey[L_ID_R]) {
        lock_decode();
      }
      if (processorKey[ID_R]) {
        decode();
      }
      if (processorKey[B_IF_R]) {
        branch_fetch();
      }
      if (processorKey[IF_R]) {
        fetch();
      }
      cyclesThisRun++;
      clock_cycles++;
    }
    break;
  }
}

/* reset the state of the pipeline simulator */
void sim_pipe::reset() {
    /* Clear gp registers */
    for(int i = 0; i<NUM_GP_REGISTERS; i++){
        gp_registers[i] = UNDEFINED;
    }
    /* Clear data memory */
    for(int i = 0; i<data_memory_size;i++){
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

// return value of special purpose register
unsigned sim_pipe::get_sp_register(sp_register_t reg, stage_t s) {
  /* pipeline object has 4 stages, processor has 5 stages. Return sp register of
   * pipeline object preceding the stage argument*/
  if(s == IF){
    switch(reg){
      case PC:
        return pipeline.stage[s].spRegisters[PIPELINE_PC];
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
  return stalls; // please modify
}

unsigned sim_pipe::get_clock_cycles() {
  return clock_cycles; // please modify
}
