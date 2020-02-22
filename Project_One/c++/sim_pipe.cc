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
int processorKey[5] = {1,0,0,0,0};
int processorKeyNext[5] = {1,0,0,0,0};
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
  for (int i = 0; i < NUM_STAGES; i++) {
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
bool is_branching_instruction(opcode_t a_opcode){
  switch (a_opcode){
  case BEQZ:
  case BNEZ:
  case BGTZ:
  case BGEZ:
  case BLTZ:
  case BLEZ:
    return true;
  default:
    return false;
  }
}
bool sim_pipe::get_program_complete(){
  return program_complete;
}

void sim_pipe::fetch() {
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

/*Determine next instruction to fetch: this is an issue b/c next instruction isn't alwasy immediately available
 * For example, if the instruction we just fetched is a branching instruction, the next fetch won't be known for two clock cycles
 * Current structure hinges on a pulling of an instruction to determine what instruction should be pulled next.*/

  unsigned branchingCond = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND];
  if(stallsNeeded){
    currentInstruction = stallInstruction;
  }
  else{
    fetchInstruction = pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC];
    /* set fetchInstruction to a value that can be used as an index
     * for instr_memory[] */
    fetchInstructionIndex = (fetchInstruction - 0x10000000) / 4;
    currentInstruction = instr_memory[fetchInstructionIndex];
  }

  /* Must check for branch instructions */
  if(is_branching_instruction(currentInstruction.opcode)){
    /* insert stalls */
    stallsNeeded = 2;
    /******if stall is needed, values is set here******/
    /* All branching conditions require two stalls. How to insert stall? next
     * instruction should be NOP. NOP instruction only needs opcode */
  }

  /*<<Determining PC>>*/
  /* switch on opcode */
  switch(currentInstruction.opcode){
  /* if NOP, check/decrement stalls */
    case NOP:
    /******And immediately decrmenented here. Will this be a problem????******/
      /* if stalls are still needed, next instruction needs to be another stall */
      if (!stallsNeeded--) {
        /******I don't think so. The logic still make sense. The number of
         * stalls is decrmented as a stall is inserted******/
        // pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
        if (pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND]) {
          /*get label*/
          string someString = pipeline.stage[EXE_MEM].parsedInstruction.label;
          /*compare label to labels in instruction memory*/
          for(int i = 0; i<PROGRAM_SIZE;i++){
            if(instr_memory[i].label==someString){
          /*convert index to usable form*/
              valuePassedAsPC = (i*4)+0x10000000;
            }
          }
        }
        /*if conditional evaluated as false, the next fetch should be
           instruction immediately after the branch instruction, I think its the
           branch instructions NPC*/
        else {
          /* need to make sure that when NOP is propogated through pipeline, the
           * pipeline registers are held static */
          valuePassedAsPC = pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC];
        }
      }
      /* if no additional stalls are needed, conditional evaluation is ready,
         branch instructions target address can be determined */
      break;
      case EOP:
        /*Set pc to undefined??*/
        pipeline.stage[IF_ID].spRegisters[PIPELINE_PC] = UNDEFINED;
        /*Set npc to undefined??*/
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] =
            pipeline.stage[IF_ID].spRegisters[PIPELINE_PC];
        /*Set fetcher to zero*/
        processorKeyNext[IF] = 0;
        break;
    default:
        /*Determining PC for a non branching instruction*/
      valuePassedAsPC = fetchInstruction + 4;
      break;
  }
  /*Update PC register*/
    pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = valuePassedAsPC;
  /*Update NPC register*/
    pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;



    pipeline.stage[IF_ID].parsedInstruction = currentInstruction;


    processorKeyNext[ID]++;

  ///////////////////////////////////////////////////////////////////
  // /*Handle EOP instruction*/                                    //
  // if (pipeline.stage[IF_ID].parsedInstruction.opcode == EOP) {  //
  // /*Set pc to undefined??*/                                     //
  //   pipeline.stage[IF_ID].spRegisters[PIPELINE_PC] = UNDEFINED; //
  // /*Set npc to undefined??*/                                    //
  //   pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] =              //
  //       pipeline.stage[IF_ID].spRegisters[PIPELINE_PC];         //
  // /*Set fetcher to zero*/                                       //
  //   processorKeyNext[IF] = 0;                                   //
  // }                                                             //
  ///////////////////////////////////////////////////////////////////
  ///////////////////BEFORE CONDITIONAL HAZARDS WERE ADDRESSED//////////////////////
  // switch (branchingCond) {                                                     //
  // case 1:                                                                      //
  //   /* if branch condition is zero, PC is now the ALU output of the branch     //
  //    * instruction */                                                          //
  //   pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] =                             //
  //       pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];                  //
  //   break;                                                                     //
  //   /*branchingCond = 0 or UNDEFINED*/                                         //
  // default:                                                                     //
  //   pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = fetchInstruction + 4; //
  //   pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] =                             //
  //     pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC];                      //
  //   break;                                                                     //
  // }                                                                            //
  //////////////////////////////////////////////////////////////////////////////////
}
void sim_pipe::decode()
{
  /*Function to parse the register file into special purpose registers
   */
  /*forward instruction register from ID_EXE stage*/
  instruction_t currentInstruction = pipeline.stage[IF_ID].parsedInstruction;
  pipeline.stage[ID_EXE].parsedInstruction = currentInstruction;
  /*different functions pass different values through the sp registers. This is
   * the start of conditional logic to determine which values should be pushed
   * through, but it is incomplete*/
  switch (currentInstruction.opcode)
  {
    case SW:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = gp_registers[currentInstruction.src2];
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = currentInstruction.immediate;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
      break;
  case LW:
    /*don't pass A*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = currentInstruction.immediate;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  case ADD:
  case SUB:
  case XOR:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] =gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = gp_registers[currentInstruction.src2];
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = UNDEFINED;
    /*pass NPC (I don't think this value will change)*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] =
        pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
    break;
  case ADDI:
  case SUBI:
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = gp_registers[currentInstruction.src1];
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = currentInstruction.immediate;
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
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = currentInstruction.src1;
    /*don't pass B*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
    /*pass immediate reg*/
    pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = currentInstruction.immediate;
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
  //decrement number of decodes stages needed
  processorKeyNext[ID]--;
  //increment number of execute stages needed
  processorKeyNext[EXE]++;
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
  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND] = conditional_evaluation(
      pipeline.stage[ID_EXE].spRegisters[ID_EXE_A],
      pipeline.stage[EXE_MEM].parsedInstruction.opcode);

  /*take ALU action*/
  unsigned currentALUOutput = alu(currentInstruction.opcode,executeA,executeB,executeImm,executeNPC);
  
  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT] = currentALUOutput;
  // decrement number of execute stages needed
  processorKeyNext[EXE]--;
  // increment number of memory stages needed
  processorKeyNext[MEM]++;
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
  // propogate ALU output
  unsigned currentALUOutput =
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
  pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;

switch(currentOpcode){
    case LW:
      /*Determine load value by referencing the data_memory array, at index generated by the ALU*/
      whatToLoad = &data_memory[currentALUOutput];
      /*Pass the reference of the load value to conversion function
       * Pass conversion function ot next stage of pipeline*/
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = char2int(whatToLoad);
      break;
    case SW:
      /*Pass memory address (pulled from register, processed with ALU) to conversion function
        * THIS conversion function handles writing to memory (store-word doesn't need a write_back() call)
       */
      int2char(currentB,data_memory+currentALUOutput);
      break;
    default:
      break;
  }
  // decrement number of memory stages needed
  processorKeyNext[MEM]--;
  // Conditionally increment number of write back stages needed
  processorKeyNext[WB]++;
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
  else{
    instructions_executed++;
  }
  processorKeyNext[WB]--;
}
/* body of the simulator */
void sim_pipe::run(unsigned cycles) {
  switch (cycles) {
  case CYCLES_NOT_DECLARED:
    while (!program_complete) {
      processor_key_update();
      if (processorKey[WB]) {
        write_back();
      }
      if (processorKey[MEM]) {
        memory();
      }
      if (processorKey[EXE]) {
        execute();
      }
      if (processorKey[ID]) {
        decode();
      }
      if (processorKey[IF]) {
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
       * the number of functions called also changes I'm struggline with
       * implementing changing the number of actions taken within the while
       * loop*/
      // trying with this stupid array check and copy system
      processor_key_update();
      if (processorKey[WB]) {
        write_back();
      }
      if (processorKey[MEM]) {
        memory();
      }
      if (processorKey[EXE]) {
        execute();
      }
      if (processorKey[ID]) {
        decode();
      }
      if (processorKey[IF]) {
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
    for(int i = 0; i<NUM_STAGES-1;i++){
        for(int j = 0; j< NUM_SP_REGISTERS; j++){
          pipeline.stage[i].spRegisters[j] = UNDEFINED;
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
