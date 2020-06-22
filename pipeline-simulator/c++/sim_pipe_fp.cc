#include "sim_pipe_fp.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>

// NOTE: structural hazards on MEM/WB stage not handled
//====================================================

//#define DEBUG
//#define DEBUG_MEMORY

using namespace std;

// used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {
    "PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {
    "LW",   "SW",   "ADD",  "ADDI", "SUB",   "SUBI", "XOR", "BEQZ",
    "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ",  "JUMP", "EOP", "NOP",
    "LWS",  "SWS",  "ADDS", "SUBS", "MULTS", "DIVS"};
static const char *unit_names[4] = {"INTEGER", "ADDER", "MULTIPLIER",
                                    "DIVIDER"};
int number_of_stalls = 0;
int number_of_cycles = 0;
int dep_stalls_needed = 0;
int control_stall=0;
int number_of_control_stalls=0;
int cycles_run = 0;
int control_stalls_inserted=0;
/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */

/* convert a float into an unsigned */
inline unsigned float2unsigned(float value) {
  unsigned result;
  memcpy(&result, &value, sizeof value);
  return result;
}

/* convert an unsigned into a float */
inline float unsigned2float(unsigned value) {
  float result;
  memcpy(&result, &value, sizeof value);
  return result;
}

/* convert integer into array of unsigned char - little indian */
inline void unsigned2char(unsigned value, unsigned char *buffer) {
  buffer[0] = value & 0xFF;
  buffer[1] = (value >> 8) & 0xFF;
  buffer[2] = (value >> 16) & 0xFF;
  buffer[3] = (value >> 24) & 0xFF;
}

/* convert array of char into integer - little indian */
inline unsigned char2unsigned(unsigned char *buffer) {
  return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/* the following functions return the kind of the considered opcode */

bool is_branch(opcode_t opcode) {
  return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ ||
          opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode) {
  return (opcode == LW || opcode == SW || opcode == LWS || opcode == SWS);
}

bool is_int_r(opcode_t opcode) {
  return (opcode == ADD || opcode == SUB || opcode == XOR);
}

bool is_int_imm(opcode_t opcode) { return (opcode == ADDI || opcode == SUBI); }

bool is_int_alu(opcode_t opcode) {
  return (is_int_r(opcode) || is_int_imm(opcode));
}

bool is_fp_alu(opcode_t opcode) {
  return (opcode == ADDS || opcode == SUBS || opcode == MULTS ||
          opcode == DIVS);
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
  case LWS:
  case SWS:
    return (a + imm);
  case BEQZ:
  case BNEZ:
  case BGTZ:
  case BGEZ:
  case BLTZ:
  case BLEZ:
  case JUMP:
    return (npc + imm);
  case ADDS:
    return (float2unsigned(unsigned2float(a) + unsigned2float(b)));
    break;
  case SUBS:
    return (float2unsigned(unsigned2float(a) - unsigned2float(b)));
    break;
  case MULTS:
    return (float2unsigned(unsigned2float(a) * unsigned2float(b)));
    break;
  case DIVS:
    return (float2unsigned(unsigned2float(a) / unsigned2float(b)));
    break;
  default:
    return (-1);
  }
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* ============== primitives to allocate/free the simulator ==================
 */

sim_pipe_fp::sim_pipe_fp(unsigned mem_size, unsigned mem_latency) {
  data_memory_size = mem_size;
  data_memory_latency = mem_latency;
  data_memory = new unsigned char[data_memory_size];
  num_units = 0;
  reset();
}

sim_pipe_fp::~sim_pipe_fp() { delete[] data_memory; }

/* =============   primitives to print out the content of the memory & registers
 * and for writing to memory ============== */

void sim_pipe_fp::print_memory(unsigned start_address, unsigned end_address) {
  cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address
       << ":0x" << hex << setw(8) << setfill('0') << end_address << "]" << endl;
  for (unsigned i = start_address; i < end_address; i++) {
    if (i % 4 == 0)
      cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
    cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
    if (i % 4 == 3) {
#ifdef DEBUG_MEMORY
      unsigned u = char2unsigned(&data_memory[i - 3]);
      cout << " - unsigned=" << u << " - float=" << unsigned2float(u);
#endif
      cout << endl;
    }
  }
}

void sim_pipe_fp::write_memory(unsigned address, unsigned value) {
  unsigned2char(value, data_memory + address);
}

void sim_pipe_fp::print_registers() {
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
    if (get_int_register(i) != (int)UNDEFINED)
      cout << "R" << dec << i << " = " << get_int_register(i) << hex << " / 0x"
           << get_int_register(i) << endl;
  for (i = 0; i < NUM_GP_REGISTERS; i++)
    if (get_fp_register(i) != UNDEFINED)
      cout << "F" << dec << i << " = " << get_fp_register(i) << hex << " / 0x"
           << float2unsigned(get_fp_register(i)) << endl;
}

/* =============   primitives related to the functional units ============== */

/* initializes an execution unit */
void sim_pipe_fp::init_exec_unit(exe_unit_t exec_unit, unsigned latency,
                                 unsigned instances) {
  for (unsigned i = 0; i < instances; i++) {
    exec_units[num_units].type = exec_unit;
    exec_units[num_units].latency = latency;
    exec_units[num_units].busy = 0;
    exec_units[num_units].instruction.opcode = NOP;
    num_units++;
  }
}

/* returns a free unit for that particular operation or UNDEFINED if no unit is
 * currently available */
unsigned sim_pipe_fp::get_free_unit(opcode_t opcode) {
  if (num_units == 0) {
    cout << "ERROR:: simulator does not have any execution units!\n";
    exit(-1);
  }
  for (unsigned u = 0; u < num_units; u++) {
    switch (opcode) {
    // Integer unit
    case NOP:
    case LW:
    case SW:
    case ADD:
    case ADDI:
    case SUB:
    case SUBI:
    case XOR:
    case BEQZ:
    case BNEZ:
    case BLTZ:
    case BGTZ:
    case BLEZ:
    case BGEZ:
    case JUMP:
    case LWS:
    case SWS:
      if (exec_units[u].type == INTEGER && exec_units[u].busy == 0)
        return u;
      break;
    // FP adder
    case ADDS:
    case SUBS:
      if (exec_units[u].type == ADDER && exec_units[u].busy == 0)
        return u;
      break;
    // Multiplier
    case MULTS:
      if (exec_units[u].type == MULTIPLIER && exec_units[u].busy == 0)
        return u;
      break;
    // Divider
    case DIVS:
      if (exec_units[u].type == DIVIDER && exec_units[u].busy == 0)
        return u;
      break;
    default:
      cout << "ERROR:: operations not requiring exec unit!\n";
      exit(-1);
    }
  }
  return UNDEFINED;
}

/* decrease the amount of clock cycles during which the functional unit will be
 * busy - to be called at each clock cycle  */
void sim_pipe_fp::decrement_units_busy_time() {
  for (unsigned u = 0; u < num_units; u++) {
    if (exec_units[u].busy > 0)
      exec_units[u].busy--;
  }
}

/* prints out the status of the functional units */
void sim_pipe_fp::debug_units() {
  for (unsigned u = 0; u < num_units; u++) {
    cout << " -- unit " << unit_names[exec_units[u].type]
         << " latency=" << exec_units[u].latency
         << " busy=" << exec_units[u].busy
         << " instruction=" << instr_names[exec_units[u].instruction.opcode]
         << endl;
  }
}

/* ========= end primitives related to functional units ===============*/

/* ========================parser ==================================== */

void sim_pipe_fp::load_program(const char *filename, unsigned base_address) {

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
    case ADDS:
    case SUBS:
    case MULTS:
    case DIVS:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      par3 = strtok(NULL, " \t");
      instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
      instr_memory[instruction_nr].src1 = atoi(strtok(par2, "RF"));
      instr_memory[instruction_nr].src2 = atoi(strtok(par3, "RF"));
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
    case LWS:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
      instr_memory[instruction_nr].immediate =
          strtoul(strtok(par2, "()"), NULL, 0);
      instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
      break;
    case SW:
    case SWS:
      par1 = strtok(NULL, " \t");
      par2 = strtok(NULL, " \t");
      instr_memory[instruction_nr].src1 = atoi(strtok(par1, "RF"));
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

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */

/* simulator */
void sim_pipe_fp::run(unsigned cycles) {
  if (!cycles) {
    write_back();
    memory();
    if (!STRUCT_HAZ_MEM_STALL) {
      execute();
      normal_decode();
      if (!DATA_RAW_STALL && !DATA_WAW_STALL && !STRUCT_HAZ_EXE_STALL) {
        fetch();
      }
    }
    if (DATA_RAW_STALL || DATA_WAW_STALL || STRUCT_HAZ_EXE_STALL ||
        STRUCT_HAZ_MEM_STALL) {
      number_of_stalls++;
    }
    DATA_RAW_STALL = 0;
    DATA_WAW_STALL = 0;
    STRUCT_HAZ_EXE_STALL = 0;
    number_of_cycles++;
  } else {
    while (cycles_run < cycles) {
      write_back();
      memory();
      if (!STRUCT_HAZ_MEM_STALL) {
        execute();
        normal_decode();
        if (!DATA_RAW_STALL && !DATA_WAW_STALL && !STRUCT_HAZ_EXE_STALL) {
          fetch();
        }
      }
      if (DATA_RAW_STALL || DATA_WAW_STALL || STRUCT_HAZ_EXE_STALL ||
          STRUCT_HAZ_MEM_STALL) {
        number_of_stalls++;
      }
      DATA_RAW_STALL = 0;
      DATA_WAW_STALL = 0;
      STRUCT_HAZ_EXE_STALL = 0;
      number_of_cycles++;
      cycles_run++;
    }
    cycles_run = 0;
  }
}

// reset the state of the sim_pipe_fpulator
void sim_pipe_fp::reset() {
  // init data memory
  for (unsigned i = 0; i < data_memory_size; i++)
    data_memory[i] = (float)0xFF;
  // init instruction memory
  for (int i = 0; i < PROGRAM_SIZE; i++) {
    instr_memory[i].opcode = (opcode_t)NOP;
    instr_memory[i].src1 = UNDEFINED;
    instr_memory[i].src2 = UNDEFINED;
    instr_memory[i].dest = UNDEFINED;
    instr_memory[i].immediate = UNDEFINED;
    instr_memory[i].type = INTEGER;
  }
  for (int i = 0; i < NUM_GP_REGISTERS; i++) {
    gp_int_registers[i] = UNDEFINED;
    gp_float_registers[i] = float2unsigned(UNDEFINED);
  }

  /* complete the reset function here */
  for (int i = 0; i < MAX_UNITS; i++) {
    exec_units[i].busy = false;
    exec_units[i].latency = UNDEFINED;
    exec_units[i].instruction = {NOP};
  }
  for(int i = 0; i<NUM_STAGES; i++){
    pipeline.stage[i].parsedInstruction={NOP,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,""};
    for(int j = 0; j< NUM_SP_REGISTERS;j++){
      pipeline.stage[i].spRegisters[j]=UNDEFINED;
    }
  }
}

// return value of special purpose register
unsigned sim_pipe_fp::get_sp_register(sp_register_t reg, stage_t s) {
  /* pipeline object has 4 stages, processor has 5 stages. Return sp register of
   * pipeline object preceding the stage argument*/
  if (s == IF) {
    switch (reg) {
    case PC:
      return pipeline.stage[s].spRegisters[PIPELINE_PC];
      break;
    default:
      return UNDEFINED;
      break;
    }
  }
  if (s == ID) {
    switch (reg) {
    case NPC:
      return pipeline.stage[s].spRegisters[IF_ID_NPC];
      break;
    default:
      return UNDEFINED;
      break;
    }
  }
  if (s == EXE) {
    switch (reg) {
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
  if (s == MEM) {
    switch (reg) {
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
  if (s == WB) {
    switch (reg) {
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
  } else {
    return 0xff;
  }
}

int sim_pipe_fp::get_int_register(unsigned reg) {
  return gp_int_registers[reg]; // please modify
}

void sim_pipe_fp::set_int_register(unsigned reg, int value) {
  gp_int_registers[reg] = value;
}

float sim_pipe_fp::get_fp_register(unsigned reg) {

  return unsigned2float(gp_float_registers[reg]); // please modify
}

bool writes_to_FP(opcode_t opcode){
  return (opcode == LWS || opcode == SWS ||opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS);
}
bool writes_to_GP(opcode_t opcode){
  return (opcode == ADD || opcode == SUB || opcode == XOR || opcode == LW || opcode == SW || opcode == ADDI || opcode == SUBI);
}
bool reads_from_GP(opcode_t opcode){
  return (opcode == ADD || opcode == SUB || opcode == XOR || opcode == LW || opcode == SW|| opcode == LWS || opcode == SWS || opcode == SUBI || opcode == ADDI || is_branch(opcode));
}
bool reads_from_FP(opcode_t opcode){
  return (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS);
}
void sim_pipe_fp::set_fp_register(unsigned reg, float value) {
  gp_float_registers[reg] = float2unsigned(value);
}

float sim_pipe_fp::get_IPC() {
  return instructions_executed / clock_cycles; // please modify
}

unsigned sim_pipe_fp::get_instructions_executed() {

  return instructions_executed; // please modify
}

unsigned sim_pipe_fp::get_clock_cycles() {

  return clock_cycles; // please modify
}

unsigned sim_pipe_fp::get_stalls() {
  if(control_stalls_inserted){
    return number_of_stalls+number_of_control_stalls;
  }
  else{
    return number_of_stalls;
  }
}

instruction_t sim_pipe_fp::get_instruction(pipeline_stage_t s) {
  return pipeline.stage[s].parsedInstruction;
}


void sim_pipe_fp::write_back() {
  // put whats in the alu output register into the destination that
  // is in the destination register
  // if load instruction, pass LMD to register
  // if arithmatic, pass alu output
  // either store word or load word happens in the memory stage, not sure which
  // one
  instruction_t currentInstruction = pipeline.stage[MEM_WB].parsedInstruction;
  opcode_t currentOpcode = currentInstruction.opcode;
  unsigned currentALUOutput =
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT];
  unsigned currentLMD = pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD];
  int registerIndex = currentInstruction.dest;
  bool intWriteBackNeeded = false;
  bool fpWriteBackNeeded = false;
  switch (currentOpcode) {
  case LW:
  case ADD:
  case SUB:
  case ADDI:
  case SUBI:
  case XOR:
    intWriteBackNeeded = true;
    break;
  case LWS:
  case ADDS:
  case MULTS:
  case DIVS:
  case SUBS:
    fpWriteBackNeeded = true;
    break;
  default:
    break;
  }
  if (intWriteBackNeeded) {
    switch (currentOpcode) {
    case LW:
      gp_int_registers[registerIndex] = currentLMD;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
      break;
    default:
      gp_int_registers[registerIndex] = currentALUOutput;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
      break;
    }
  }
  if (fpWriteBackNeeded) {
    switch (currentOpcode) {
    case LWS:
      gp_float_registers[registerIndex] = currentLMD;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
      break;
    default:
      gp_float_registers[registerIndex] = currentALUOutput;
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
      break;
    }
  }
  if (currentInstruction.opcode == EOP) {
    program_complete = true;
    pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = UNDEFINED;
  }
  if (currentInstruction.opcode != NOP && currentInstruction.opcode != EOP) {
    instructions_executed++;
  }
}

void sim_pipe_fp::fetch() {
  unsigned fetchInstruction;
  unsigned fetchInstructionIndex;
  unsigned valuePassedAsPC;
  instruction_t currentInstruction;
  instruction_t checkInstruction;
  checkInstruction.immediate=pipeline.stage[MEM_WB].parsedInstruction.immediate;
  checkInstruction.dest=pipeline.stage[MEM_WB].parsedInstruction.dest;
  checkInstruction.src2=pipeline.stage[MEM_WB].parsedInstruction.src2;
  checkInstruction.src1=pipeline.stage[MEM_WB].parsedInstruction.src1;
  checkInstruction.opcode=pipeline.stage[MEM_WB].parsedInstruction.opcode;
  /*IDK if the following declaration will work */
  instruction_t nextInstruction = {NOP, UNDEFINED, UNDEFINED,
                                           UNDEFINED, UNDEFINED, ""};
  static int stallsNeeded = 2;
  static int potentialNPC;
  static bool insertStall = false;
  static int stallCounter = 0;
  bool skip = false;
  /*Determine next instruction to fetch: this is an issue b/c next instruction
   * isn't alwasy immediately available For example, if the instruction we just
   * fetched is a branching instruction, the next fetch won't be known for two
   * clock cycles Current structure hinges on a pulling of an instruction to
   * determine what instruction should be pulled next.*/

  /*Arith/Load/Store Instructions*/
  /*Get current PC*/
  fetchInstruction = pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC];
  /*branch instruction has made it to the end*/
  if (checkInstruction.opcode > XOR && checkInstruction.opcode < EOP) {
    control_stall = 0;
    number_of_control_stalls++;
    if (pipeline.stage[MEM_WB_COND].spRegisters[MEM_WB_COND] == 1) {
      fetchInstruction =
          pipeline.stage[MEM_WB_COND].spRegisters[MEM_WB_ALU_OUT];
    }
  }
  fetchInstructionIndex = (fetchInstruction - instr_base_address) / 4;
  /* set fetchInstruction to a value that can be used as an index
   * for instr_memory[] */
  /*get current instruction*/
  nextInstruction = instr_memory[fetchInstructionIndex];
  /*Set NPC*/
  valuePassedAsPC = fetchInstruction + 4;
  /*Check if branch*/
  if(control_stall){
    valuePassedAsPC = valuePassedAsPC -4;
    nextInstruction = {NOP,UNDEFINED,UNDEFINED,UNDEFINED,UNDEFINED,""};
  }
  else if(nextInstruction.opcode==EOP){
    valuePassedAsPC = valuePassedAsPC -4;
  }
  pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = valuePassedAsPC;
  if(control_stall){
    valuePassedAsPC = UNDEFINED;
  }
  pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;
  pipeline.stage[IF_ID].parsedInstruction=nextInstruction;
  if(nextInstruction.opcode>XOR && nextInstruction.opcode<EOP){
    number_of_control_stalls++;
    control_stall=1;
    control_stalls_inserted = 1;
  }
}

void sim_pipe_fp::normal_decode() {
  unsigned checkA, checkB, checkNPC, checkImmediate;
  instruction_t checkInstruction;
  checkInstruction.immediate = pipeline.stage[IF_ID].parsedInstruction.immediate;
  checkInstruction.opcode = pipeline.stage[IF_ID].parsedInstruction.opcode;
  checkInstruction.src1 = pipeline.stage[IF_ID].parsedInstruction.src1;
  checkInstruction.src2 = pipeline.stage[IF_ID].parsedInstruction.src2;
  checkInstruction.label = pipeline.stage[IF_ID].parsedInstruction.label;
  checkInstruction.dest = pipeline.stage[IF_ID].parsedInstruction.dest;
  checkInstruction.type = pipeline.stage[IF_ID].parsedInstruction.type;

  // pipeline.stage[ID_EXE].parsedInstruction = currentInstruction;
  /*different functions pass different values through the sp registers. This
   * is the start of conditional logic to determine which values should be
   * pushed through, but it is incomplete*/
  switch (checkInstruction.opcode) {
  case SW:
    checkA = get_int_register(checkInstruction.src2);
    checkB = get_int_register(checkInstruction.src1);
    checkInstruction.type = INTEGER;
    checkInstruction.dest = UNDEFINED;
    break;
  case SWS:
    checkA = get_int_register(checkInstruction.src2);
    checkB = float2unsigned(get_fp_register(checkInstruction.src1));
    checkInstruction.dest = UNDEFINED;
    checkInstruction.type = INTEGER;
    break;
  case LW:
  case LWS:
    /*don't pass A*/
    checkA = get_int_register(checkInstruction.src1);
    checkB = UNDEFINED;
    checkInstruction.type = INTEGER;
    break;
  case ADD:
  case SUB:
  case XOR:
    checkA = gp_int_registers[checkInstruction.src1];
    checkB = gp_int_registers[checkInstruction.src2];
    checkInstruction.immediate = UNDEFINED;
    checkInstruction.type = INTEGER;
    break;
  case ADDI:
  case SUBI:
    checkA = gp_int_registers[checkInstruction.src1];
    checkB = UNDEFINED;
    checkInstruction.type = INTEGER;
    break;
  case BEQZ:
  case BGEZ:
  case BGTZ:
  case BLEZ:
  case BLTZ:
  case BNEZ:
  case JUMP:
    checkA = gp_int_registers[checkInstruction.src1];
    checkB = UNDEFINED;
    checkInstruction.dest = UNDEFINED;
    checkInstruction.type = INTEGER;
    break;
  case ADDS:
  case SUBS:
    checkInstruction.type = ADDER;
    checkA = float2unsigned(get_fp_register(checkInstruction.src1));
    checkB = float2unsigned(get_fp_register(checkInstruction.src2));
    checkInstruction.immediate = UNDEFINED;
    break;
  case MULTS:
    checkInstruction.type = MULTIPLIER;
    checkA = float2unsigned(get_fp_register(checkInstruction.src1));
    checkB = float2unsigned(get_fp_register(checkInstruction.src2));
    checkInstruction.immediate = UNDEFINED;
    break;
  case DIVS:
    checkInstruction.type = DIVIDER;
    checkA = float2unsigned(get_fp_register(checkInstruction.src1));
    checkB = float2unsigned(get_fp_register(checkInstruction.src2));
    checkInstruction.immediate = UNDEFINED;
    break;
  default:
    checkB = UNDEFINED;
    checkA = UNDEFINED;
    checkInstruction.immediate = UNDEFINED;
    checkInstruction.type=INTEGER;
    break;
  }
  checkNPC = pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
  checkImmediate = checkInstruction.immediate;
  RAW_check();
  WAW_check(checkInstruction.dest,checkInstruction.type);

  if (get_free_unit(checkInstruction.opcode) == UNDEFINED) {
    STRUCT_HAZ_EXE_STALL = 1;
  }

  if (STRUCT_HAZ_EXE_STALL || DATA_RAW_STALL ||
      DATA_WAW_STALL) {
    checkA = UNDEFINED;
    checkB = UNDEFINED;
    checkImmediate = UNDEFINED;
    checkNPC = UNDEFINED;
    checkInstruction.opcode = NOP;
    checkInstruction.src1 = UNDEFINED;
    checkInstruction.src2 = UNDEFINED;
    checkInstruction.dest = UNDEFINED;
    checkInstruction.immediate = UNDEFINED;
    checkInstruction.label = "";
  }

  pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = checkA;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = checkB;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] = checkNPC;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = checkImmediate;
  pipeline.stage[ID_EXE].parsedInstruction.opcode = checkInstruction.opcode;
  pipeline.stage[ID_EXE].parsedInstruction.src2= checkInstruction.src2;
  pipeline.stage[ID_EXE].parsedInstruction.src1= checkInstruction.src1;
  pipeline.stage[ID_EXE].parsedInstruction.immediate = checkInstruction.immediate;
  pipeline.stage[ID_EXE].parsedInstruction.type= checkInstruction.type;
  pipeline.stage[ID_EXE].parsedInstruction.dest= checkInstruction.dest;
}

void sim_pipe_fp::execute() {
  /*ID_EXE -> EXE_MEM*/
  /* Forward instruction register from  */
  instruction_t currentInstruction;

  currentInstruction.immediate = pipeline.stage[ID_EXE].parsedInstruction.immediate;
  currentInstruction.opcode = pipeline.stage[ID_EXE].parsedInstruction.opcode;
  currentInstruction.src1 = pipeline.stage[ID_EXE].parsedInstruction.src1;
  currentInstruction.src2 = pipeline.stage[ID_EXE].parsedInstruction.src2;
  currentInstruction.label = pipeline.stage[ID_EXE].parsedInstruction.label;
  currentInstruction.dest = pipeline.stage[ID_EXE].parsedInstruction.dest;
  currentInstruction.type = pipeline.stage[ID_EXE].parsedInstruction.type;
  /* get A to use in this stage */
  unsigned executeA = pipeline.stage[ID_EXE].spRegisters[ID_EXE_A];
  /* get B to use in this stage */
  unsigned executeB = pipeline.stage[ID_EXE].spRegisters[ID_EXE_B];
  /* get NPC to use in this stage */
  unsigned executeNPC = pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC];

  unsigned latency;

  unsigned pendingCompletion = UNDEFINED;

  pipeline_alu completedExecution;

  /* Generate conditional output */
  /* But don't overwrite current conditional output if its needed */
  if (currentInstruction.opcode != EOP &&  currentInstruction.opcode!=NOP) {
    unsigned executionIndex = get_free_unit(currentInstruction.opcode);
    pipeline_alu tempRegister;
    latency = exec_units[executionIndex].latency;
    tempRegister.out = alu(currentInstruction.opcode, executeA, executeB,
                           currentInstruction.immediate, executeNPC);

      tempRegister.aluB = executeB;
      tempRegister.cond =
          conditional_evaluation(executeA, currentInstruction.opcode);
      tempRegister.instruction.type = currentInstruction.type;
      tempRegister.instruction.dest = currentInstruction.dest;
      tempRegister.instruction.label = currentInstruction.label;
      tempRegister.instruction.src1 = currentInstruction.src1;
      tempRegister.instruction.src2 = currentInstruction.src2;
      tempRegister.instruction.opcode= currentInstruction.opcode;
      tempRegister.instruction.immediate = currentInstruction.immediate;

      exec_units[executionIndex].thisALU =
          tempRegister;

      exec_units[executionIndex].busy=(latency+1);

  }
  for (int i = 0; i < num_units; i++) {
    if (exec_units[i].busy == 1) {
      pendingCompletion = 1;
      completedExecution = exec_units[i].thisALU;
      exec_units[i].thisALU.instruction={NOP,UNDEFINED, UNDEFINED,
                               UNDEFINED, UNDEFINED, ""};
    }
  }
  if (pendingCompletion == UNDEFINED) {
    completedExecution.cond = UNDEFINED;
    completedExecution.out = UNDEFINED;
    completedExecution.aluB = UNDEFINED;
  }
  decrement_units_busy_time();

  if(pipeline.stage[EXE_MEM].clear_to_write){
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT] =
        completedExecution.out;
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B] = completedExecution.aluB;
    pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND] = completedExecution.cond;

    pipeline.stage[EXE_MEM].parsedInstruction.type =
        completedExecution.instruction.type;
    pipeline.stage[EXE_MEM].parsedInstruction.dest =
        completedExecution.instruction.dest;
    pipeline.stage[EXE_MEM].parsedInstruction.label =
        completedExecution.instruction.label;
    pipeline.stage[EXE_MEM].parsedInstruction.src1 =
        completedExecution.instruction.src1;
    pipeline.stage[EXE_MEM].parsedInstruction.src2 =
        completedExecution.instruction.src2;
    pipeline.stage[EXE_MEM].parsedInstruction.opcode =
        completedExecution.instruction.opcode;
  }
}
void sim_pipe_fp::memory() {
  static unsigned memory_stalls = 0;
  unsigned char* buffer;
  //unsigned char whatToLoadInt[4];
  unsigned whereToLoadFrom;
  unsigned loadableData;
  unsigned currentB;
  float holdVal;
  instruction_t currentInstruction;
  opcode_t currentOpcode;
  unsigned currentLMD;
//
  unsigned cond = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND];
//
  currentInstruction.immediate = pipeline.stage[EXE_MEM].parsedInstruction.immediate;
  currentInstruction.opcode = pipeline.stage[EXE_MEM].parsedInstruction.opcode;
  currentInstruction.src1 = pipeline.stage[EXE_MEM].parsedInstruction.src1;
  currentInstruction.src2 = pipeline.stage[EXE_MEM].parsedInstruction.src2;
  currentInstruction.label = pipeline.stage[EXE_MEM].parsedInstruction.label;
  currentInstruction.dest = pipeline.stage[EXE_MEM].parsedInstruction.dest;
  currentInstruction.type = pipeline.stage[EXE_MEM].parsedInstruction.type;
//
  unsigned currentALUOutput =
      pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
//
  currentLMD = UNDEFINED;
//
  currentB = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
  currentOpcode=currentInstruction.opcode;
  /*not ready*/
  if (memory_stalls < data_memory_latency &&
      (currentOpcode == SW || currentOpcode == SWS || currentOpcode == LW ||
       currentOpcode == LWS)) {
    memory_stalls++;
    currentLMD=UNDEFINED;
    currentInstruction = {NOP, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, ""};
    currentALUOutput = UNDEFINED;
    currentLMD = UNDEFINED;
    pipeline.stage[EXE_MEM].clear_to_write=0;
    cond = 0;
    STRUCT_HAZ_MEM_STALL = 1;
    // dont write to memory
  }
  /*ready*/
  else if ((currentOpcode == SW || currentOpcode == SWS ||
              currentOpcode == LW || currentOpcode == LWS)) {
    memory_stalls = 0;
    STRUCT_HAZ_MEM_STALL = 0;
    pipeline.stage[EXE_MEM].clear_to_write=1;
/*int load store*/
//store
    if (currentInstruction.opcode == SW || currentInstruction.opcode == LW) {
      if (currentInstruction.opcode == SW) {
        cout << "Inserting: " << currentB << " at " << currentALUOutput
             << " offset from base addr." << endl;
        write_memory(currentALUOutput, currentB);
	      print_memory(0xA000, 0xA028);
	      print_memory(0xB000, 0xB028);
      }
      // load
      else {
        buffer = &data_memory[currentALUOutput];
	      print_memory(0xA000, 0xA028);
	      print_memory(0xB000, 0xB028);
        currentLMD = char2unsigned(buffer);
        cout << "Preparing to load: " << currentLMD << " from memory" << endl;
        /*Pass the reference of the load value to conversion function
         * Pass conversion function ot next stage of pipeline*/
        pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = currentLMD;
      }
    }
    else if (currentInstruction.opcode == LWS ||
               currentInstruction.opcode == SWS) {
      if (currentInstruction.opcode == SWS) {
        cout << "Inserting: " << currentB << " at " << currentALUOutput
             << " offset from base addr." << endl;
        //convert to float?
        //store to float register
        //convert to unsigned
        write_memory(currentALUOutput, currentB);
        unsigned2char(currentB,data_memory+currentALUOutput);
	      print_memory(0xA000, 0xA028);
	      print_memory(0xB000, 0xB028);
      } else {
        //character array
        buffer = &data_memory[currentALUOutput];
        currentLMD = char2unsigned(buffer); //I have an array of characters that now need to be transferred to a value that an FP reg can hold
	      print_memory(0xA000, 0xA028);
	      print_memory(0xB000, 0xB028);

        //currentLMD = char2unsigned(whatToLoad);
        //currentLMD = unsigned2float(currentLMD);
        cout << "Preparing to load: " << currentLMD << " from memory" << endl;
        /*Pass the reference of the load value to conversion function
         * Pass conversion function ot next stage of pipeline*/
        pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = currentLMD;
      }
    }
  }
  pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = currentLMD;
  pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;

  pipeline.stage[MEM_WB].parsedInstruction.type = currentInstruction.type;
  pipeline.stage[MEM_WB].parsedInstruction.dest = currentInstruction.dest;
  pipeline.stage[MEM_WB].parsedInstruction.label = currentInstruction.label;
  pipeline.stage[MEM_WB].parsedInstruction.src1 = currentInstruction.src1;
  pipeline.stage[MEM_WB].parsedInstruction.src2 = currentInstruction.src2;
  pipeline.stage[MEM_WB].parsedInstruction.opcode = currentInstruction.opcode;
}

bool single_source_check(instruction_t checkInstruction) {
  if (checkInstruction.opcode == ADDI || checkInstruction.opcode == SUBI ||
      checkInstruction.opcode == LW || checkInstruction.opcode == SW) {
    return true;

  } else {
    return false;
  }
}

int sim_pipe_fp::stage_location(opcode_t checkOpcode) {
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
    return forwardStages + data_memory_latency;
    break;
  case SW:
    return forwardStages + data_memory_latency - 1;
  default:
    return 0;
  }
  return 0;
}


unsigned sim_pipe_fp::conditional_evaluation(unsigned evaluate,
                                             opcode_t condition) {
  switch (condition) {
  case BEQZ:
    return (evaluate == 0);
  case BNEZ:
    return (evaluate != 0);
  case BGTZ:
    return (evaluate > 0);
  case BGEZ:
    return (evaluate >= 0);
  case BLTZ:
    return (evaluate < 0);
  case BLEZ:
    return (evaluate <= 0);
  default:
    return 0;
  }
}

instruction_type_t sim_pipe_fp::instruction_type(opcode_t opcode) {
  if(opcode<LWS){
    return INT_INSTRUCTION;
  }
  else{
    return FP_INSTRUCTION;
  }
}

bool sim_pipe_fp::fp_reg_dependent(opcode_t opcode) {
  if (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS) {
    return true;
  } else {
    return false;
  }
}

bool sim_pipe_fp::src_dep_check(instruction_t dependentInst,
                                instruction_t hingeInst) {

  if ((dependentInst.src1 == hingeInst.dest ||
       dependentInst.src2 == hingeInst.dest) &&
      hingeInst.dest != UNDEFINED) {
    return true;
  } else {
    return false;
  }
}

bool sim_pipe_fp::WAW_check(unsigned int dest, exe_unit_t type) {
  unsigned thisLatency = UNDEFINED;
  unsigned pendingDest = UNDEFINED;
  for (int i = 0; i < num_units; i++) {
    if (exec_units[i].type == type) {
      thisLatency = exec_units[i].latency;
    }
  }
  if (thisLatency != UNDEFINED) {
    for (int i = 0; i < num_units; i++) {
      pendingDest = exec_units[i].thisALU.instruction.dest;
      if (pendingDest == dest && pendingDest != UNDEFINED) {
        if (thisLatency < exec_units[i].busy) {
          DATA_WAW_STALL = 1;
          return true;
        }
      }
    }
  }
  return false;
}

kind_of_instruction_t
sim_pipe_fp::instruction_type_check(instruction_t checkedInstruction) {
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

bool sim_pipe_fp::RAW_check() {
  instruction_t dec_inst = pipeline.stage[IF_ID].parsedInstruction;
  instruction_t mem_inst = pipeline.stage[EXE_MEM].parsedInstruction;
  instruction_t write_inst = pipeline.stage[MEM_WB].parsedInstruction;
  unsigned decodeSrc1 = dec_inst.src1;
  unsigned decodeSrc2 = dec_inst.src2;
  /*check whats in mem and wb*/
  if (dec_inst.opcode != NOP && dec_inst.opcode != EOP) {
    if (src_dep_check(dec_inst, mem_inst) ||
        src_dep_check(dec_inst,  write_inst)) {
      if( (writes_to_FP(mem_inst.opcode) && reads_from_FP(dec_inst.opcode)) || (writes_to_FP(write_inst.opcode) && reads_from_FP(dec_inst.opcode)) ||
          (writes_to_GP(mem_inst.opcode) && reads_from_GP(dec_inst.opcode)) || (writes_to_GP(write_inst.opcode) && reads_from_GP(dec_inst.opcode)) ){
          DATA_RAW_STALL = 1;
          return true;
        }
      }
    }
  /*check whats in execution units*/
  for (int i = 0; i < num_units; i++) {
    pipeline_alu checkALU = exec_units[i].thisALU;
    if (src_dep_check(dec_inst, checkALU.instruction)) {
      if ((instruction_type(checkALU.instruction.opcode) == FP_INSTRUCTION) &&
          (fp_reg_dependent(dec_inst.opcode))) {
        if ((instruction_type(checkALU.instruction.opcode) ==
             INT_INSTRUCTION) &&
            (instruction_type(dec_inst.opcode) == INT_INSTRUCTION ||
             is_branch(dec_inst.opcode) || dec_inst.opcode == LWS ||
             dec_inst.opcode == SWS)) {
          DATA_RAW_STALL = 1;
          return true;
        }
      }
    }
  }
  return false;
}
  // if


