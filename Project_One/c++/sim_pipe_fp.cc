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
  // write_back();
  // memory();
  // if (!STRUCT_HAZ_MEM_STALL) {
  //    execute();
  //    decode();
  //    if (!DATA_HAZ_LATE_READ_STALL && !DATA_HAZ_OVER_WRITE_STALL &&
  //        !STRUCT_HAZ_EXE_STALL) {
  //     fetch();
  //   }
  // }
  // if(DATA_HAZ_LATE_READ_STALL || DATA_HAZ_OVER_WRITE_STALL ||
  //     STRUCT_HAZ_EXE_STALL || STRUCT_HAZ_MEM_STALL){
  //      number_of_stalls++;
  // }
  // DATA_HAZ_OVER_WRITE_STALL = 0;
  // DATA_HAZ_LATE_READ_STALL = 0;
  // STRUCT_HAZ_EXE_STALL = 0;
  // number_of_cycles++;
}

// reset the state of the sim_pipe_fpulator
void sim_pipe_fp::reset() {
  // init data memory
  for (unsigned i = 0; i < data_memory_size; i++)
    data_memory[i] = 0xFF;
  // init instruction memory
  for (int i = 0; i < PROGRAM_SIZE; i++) {
    instr_memory[i].opcode = (opcode_t)NOP;
    instr_memory[i].src1 = UNDEFINED;
    instr_memory[i].src2 = UNDEFINED;
    instr_memory[i].dest = UNDEFINED;
    instr_memory[i].immediate = UNDEFINED;
  }
  for (int i = 0; i < NUM_GP_REGISTERS; i++) {
    gp_int_registers[i] = 0;
    gp_float_registers[i] = 0;
  }

  /* complete the reset function here */
  for (int i = 0; i < MAX_UNITS; i++) {
    exec_units[i].busy = false;
    exec_units[i].latency = UNDEFINED;
    exec_units[i].instruction = {NOP};
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

  return gp_float_registers[reg]; // please modify
}

void sim_pipe_fp::set_fp_register(unsigned reg, float value) {
  gp_float_registers[reg] = value;
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
  return stalls; // please modify
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
      gp_float_registers[registerIndex] = unsigned2float(currentALUOutput);
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = UNDEFINED;
      break;
    default:
      gp_float_registers[registerIndex] = unsigned2float(currentALUOutput);
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
  static instruction_t stallInstruction = {NOP,       UNDEFINED, UNDEFINED,
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
  /*Set NPC*/
  valuePassedAsPC = fetchInstruction + 4;
  /* set fetchInstruction to a value that can be used as an index
   * for instr_memory[] */
  fetchInstructionIndex = (fetchInstruction - 0x10000000) / 4;
  /*get current instruction*/
  currentInstruction = instr_memory[fetchInstructionIndex];
  /*Check if branch*/
  if (insertStall) {
    pipeline.stage[IF_ID].parsedInstruction = stallInstruction;
    stallCounter++;
    stalls++;
    if (stallCounter == stallsNeeded) {
      insertStall = false;
      stallCounter = 0;
      if (pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND]) {
        pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] =
            pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
      } else {
        pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] =
            pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
      }
      skip = true;
    }
  }
  if ((!insertStall) && (!skip)) {

    pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
    /*push NPC to first pipeline register*/
    pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;
    pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = valuePassedAsPC;
    if (instruction_type_check(currentInstruction) == COND_INSTR) {
      stallCounter = 0;
      insertStall = true;
    }
    if (currentInstruction.opcode == EOP) {
      /*EOP instructions*/
      /*Set pc to undefined??*/
      pipeline.stage[PRE_FETCH].spRegisters[PIPELINE_PC] = fetchInstruction;
      /*Set npc to undefined??*/
      pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] =
          pipeline.stage[IF_ID].spRegisters[PIPELINE_PC];
      /*Set fetcher to zero*/
      pipeline.stage[IF_ID].parsedInstruction = currentInstruction;
      // pipeline.stage[IF_ID].spRegisters[IF_ID_NPC] = valuePassedAsPC;
    }
  }
  /*push instruction to first pipeline register forward*/
}

void sim_pipe_fp::decode() {
  /*Function to parse the register file into special purpose registers
   */
  /*forward instruction register from ID_EXE stage*/
  /*Create variable to hold current instruction*/
  instruction_t currentInstruction = pipeline.stage[IF_ID].parsedInstruction;
  /*Create variable to hold stall instruction*/
  static unsigned lastCheckedInstruction = UNDEFINED;
  /*If the pipeline isn't already locked, chech for dependency*/
  /*RESTRUCTURING: no need to check for dep lock, because */
  if (currentInstruction.opcode != NOP && currentInstruction.opcode != EOP &&
      lastCheckedInstruction != currentInstruction.opcode) {
    depStallsNeeded = data_dep_check(currentInstruction);
  } else {
    depStallsNeeded = 0;
  }

  if (!depStallsNeeded) {
    /*create function to handle normal decode*/
    normal_decode(currentInstruction);
    lastCheckedInstruction = UNDEFINED;
  } else {
    /*handle stall decode*/
    /*after calling lock decode*/
    lastCheckedInstruction = currentInstruction.opcode;
    lock_decode();
  }
  if (currentInstruction.opcode == EOP) {
    doDecode = false;
    doLockDecode = false;
  }
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

/*Function to check if flow dependencies exist in the pipeline, return number of
 * stalls needed*/
int sim_pipe_fp::data_dep_check(instruction_t checkedInstruction) {
  /*array to hold instructions that exist further down the pipeline. I believe
   * that the only pipeline registers I'm concerned with are the decode/execute
   * register and execute/memory register. The memory/writeback register will
   * already have been processed by the time this check happens*/
  static int PRE_MEMORY = 0;
  static int PRE_WRITE_BACK = 1;
  static int FORWARD_STAGES = 2;
  int retVal = 0;

  instruction_t pipelineInstructions[FORWARD_STAGES];
  /*Need 2 stalls*/
  /*Destination of some instruction may be the source of this instruction, but
   * that may not matter*/
  /*SW R4 R0*/
  /*ADDI R5 R4 10*/
  /*Src1 of ADDI = 4, src2 of ADDI = 0*/
  /*Possible deps dest of SW*/
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
    if (single_source_check(checkedInstruction)) {
      if (pipelineInstructions[i].dest == checkedInstruction.src1) {
        if (i == PRE_MEMORY) {
          retVal = stage_location(pipelineInstructions[i].opcode);
          globalLoadDepStalls += retVal;
          stalls += retVal;
          return retVal;
        } else {
          retVal = stage_location(pipelineInstructions[i].opcode) - 1;
          globalStoreDepStalls += retVal;
          stalls++;
          return retVal;
        }
      }
    } else {
      if (pipelineInstructions[i].dest == checkedInstruction.src1 ||
          pipelineInstructions[i].dest == checkedInstruction.src2) {
        if (i == PRE_MEMORY) {
          globalLoadDepStalls += retVal;
          retVal = stage_location(pipelineInstructions[i].opcode);
          stalls += retVal;
          return retVal;
        } else {
          retVal = stage_location(pipelineInstructions[i].opcode) - 1;
          globalStoreDepStalls += retVal;
          stalls++;
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

void sim_pipe_fp::normal_decode(instruction_t currentInstruction) {
  unsigned checkA, checkB, checkNPC, checkImmediate;
  instruction_t checkInstruction;
  checkInstruction = currentInstruction;
  // pipeline.stage[ID_EXE].parsedInstruction = currentInstruction;
  /*different functions pass different values through the sp registers. This
   * is the start of conditional logic to determine which values should be
   * pushed through, but it is incomplete*/
  switch (checkInstruction.opcode) {
  case SW:
    checkA = gp_int_registers[checkInstruction.src2];
    checkB = gp_int_registers[checkInstruction.src1];
    checkInstruction.type = INTEGER;
    checkInstruction.dest = UNDEFINED;
    break;
  case SWS:
    checkA = gp_int_registers[checkInstruction.src2];
    checkB = float2unsigned(gp_float_registers[checkInstruction.src1]);
    checkInstruction.dest = UNDEFINED;
    checkInstruction.type = INTEGER;
    break;
  case LW:
  case LWS:
    /*don't pass A*/
    checkA = gp_int_registers[checkInstruction.src1];
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
    checkInstruction.immediate = currentInstruction.immediate;
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
    checkA = float2unsigned(gp_float_registers[checkInstruction.src1]);
    checkB = float2unsigned(gp_float_registers[checkInstruction.src2]);
    checkInstruction.immediate = UNDEFINED;
    break;
  case MULTS:
    checkInstruction.type = MULTIPLIER;
    checkA = float2unsigned(gp_float_registers[checkInstruction.src1]);
    checkB = float2unsigned(gp_float_registers[checkInstruction.src2]);
    checkInstruction.immediate = UNDEFINED;
    break;
  case DIVS:
    checkInstruction.type = DIVIDER;
    checkA = float2unsigned(gp_float_registers[checkInstruction.src1]);
    checkB = float2unsigned(gp_float_registers[checkInstruction.src2]);
    checkInstruction.immediate = UNDEFINED;
    break;
  default:
    checkB = UNDEFINED;
    checkA = UNDEFINED;
    checkInstruction.immediate = UNDEFINED;
    break;
  }
  checkNPC = pipeline.stage[IF_ID].spRegisters[IF_ID_NPC];
  checkImmediate = checkInstruction.immediate;
  // check_over_write_data_hazard(checkInstruction.dest,checkInstruction.type);
  // check_read_after_write_hazard();
  if (get_free_unit(checkInstruction.opcode) == UNDEFINED) {
    STRUCT_HAZ_EXE_STALL = 1;
  }
  if (STRUCT_HAZ_EXE_STALL || DATA_HAZ_LATE_READ_STALL ||
      DATA_HAZ_OVER_WRITE_STALL) {
    checkA = UNDEFINED;
    checkB = UNDEFINED;
    checkImmediate = UNDEFINED;
    checkNPC = UNDEFINED;
    checkInstruction = {NOP, UNDEFINED, UNDEFINED, UNDEFINED, UNDEFINED, ""};
  }
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = checkA;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = checkB;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] = checkNPC;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = checkImmediate;
  pipeline.stage[ID_EXE].parsedInstruction = checkInstruction;
}

void sim_pipe_fp::lock_decode() {
  /*Pass NOP instruction*/
  static instruction_t stalled_instruction;
  static int depStallsInserted = 0;
  pipeline.stage[ID_EXE].parsedInstruction = {NOP,       UNDEFINED, UNDEFINED,
                                              UNDEFINED, UNDEFINED, ""};
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_A] = UNDEFINED;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_B] = UNDEFINED;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_IMM] = UNDEFINED;
  pipeline.stage[ID_EXE].spRegisters[ID_EXE_NPC] = UNDEFINED;

  /*check to see if more stalls will be needed*/
  doExecute = true;
  if (!depStallsInserted++) {
    stalled_instruction = pipeline.stage[IF_ID].parsedInstruction;
    /*Turn fetcher off*/
    doFetch = false;
    /*Next clock cycle, return to lock_decode()*/
    doLockDecode = true;
    /*Next clock cycle, do not run normal decode*/
    doDecode = false;
  }
  if (depStallsInserted != depStallsNeeded) {
    processorKey[ID_R] = 0;
  }
  /*If the appropriate number of stalls has been inserted*/
  if (depStallsInserted == depStallsNeeded) {
    if (depStallsNeeded == 1) {
      immediateBreak = true;
    }
    // normal_decode(stalled_instruction);
    doLockDecode = false;
    /*Turn normal decoder on (for next clock cycle)*/
    doDecode = true;
    /*Turn fetcher On (for this clock cycle)*/
    // processorKey[0] = 1;
    /*Turn fetcher On (for next clock cycle)*/
    doFetch = true;
    depStallsInserted = 0;
    depStallsNeeded = 0;
  }
  immediateBreak = true;
  didLockDecode = true;
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

void sim_pipe_fp::execute() {
  /*ID_EXE -> EXE_MEM*/
  /* Forward instruction register from  */
  instruction_t currentInstruction = pipeline.stage[ID_EXE].parsedInstruction;
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
  if (instruction_type_check(currentInstruction) != NOPEOP_INSTR) {
    pipeline_alu tempRegister;
    tempRegister.out = alu(currentInstruction.opcode, executeA, executeB,
                           currentInstruction.immediate, executeNPC) {
      tempRegister.b = executeB;
      tempRegister.cond =
          conditional_evaluation(executeA, currentInstruction.opcode);
      tempRegister.instruction = currentInstruction;
      exec_units[get_free_unit(currentInstruction.opcode)].thisALU =
          tempRegister;
    }
  }
  for (int i = 0; i < num_units; i++) {
    if (exec_units[i].busy == 1) {
      pendingCompletion = 1;
      completedExecution = exec_units[i].thisALU;
      exec_units[i].thisALU = {NOP,       UNDEFINED, UNDEFINED,
                               UNDEFINED, UNDEFINED, ""};
      exec_units[i].instruction = {NOP,       UNDEFINED, UNDEFINED,
                                   UNDEFINED, UNDEFINED, ""};
    }
  }
  if (pendingCompletion == UNDEFINED) {
    completedExecution.cond = UNDEFINED;
    completedExecution.out = UNDEFINED;
    completedExecution.aluB = UNDEFINED;
  }
  decrement_units_busy_time();

  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT] = completedExecution.out;

  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B] = completedExecution.aluB;

  pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_COND] = completedExecution.cond;
  pipeline.stage[EXE_MEM].parsedInstruction = completedExecution.instruction;

  if (completedExecution.instruction.opcode == EOP) {
    doExecute = false;
  }
}

void sim_pipe_fp::memory_stall() {
  static unsigned stalledB;
  static instruction_t stalledInstruction;
  static unsigned stalledALUOutput;
  static unsigned char *whatToLoad;
  static unsigned whereToLoadFrom;
  static unsigned loadableData;

  if (!memoryStallNumber++) {
    stalledB = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
    stalledInstruction = pipeline.stage[EXE_MEM].parsedInstruction;
    stalledALUOutput = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
  }

  if (memoryStallNumber >= (data_memory_latency + 1)) {
    /*insert another stall*/
    /*Keep other units off*/
    doFetch = false;
    doDecode = false;
    doExecute = false;
    doMemory = false;
    doMemoryStall = true;
    pipeline.stage[MEM_WB].parsedInstruction = {NOP};
  } else {
    doFetch = true;
    doDecode = true;
    doExecute = true;
    doMemory = true;
    doMemoryStall = false;
    stalls++;
    /*reset memory stall counter*/
    memoryStallNumber = 0;
    switch (stalledInstruction.opcode) {
    case LW:
      /*Determine load value by referencing the data_memory array, at index
       * generated by the ALU*/
      whatToLoad = &data_memory[stalledALUOutput];
      print_memory(0, 32);
      loadableData = char2int(whatToLoad);
      cout << "Preparing to load: " << loadableData << " from memory" << endl;
      /*Pass the reference of the load value to conversion function
       * Pass conversion function ot next stage of pipeline*/
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_LMD] = loadableData;
      break;
    case SW:
      /*Pass memory address (pulled from register, processed with ALU) to
       * conversion function THIS conversion function handles writing to
       * memory (store-word doesn't need a write_back() call)
       */
      cout << "Inserting: " << stalledB << " at " << stalledALUOutput
           << " offset from base addr." << endl;
      int2char(stalledB, data_memory + stalledALUOutput);
      print_memory(0, 32);
      break;
    default:
      break;
    }
    pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = stalledALUOutput;
    processorKeyNext[WB_R]++;
  }
}

void sim_pipe_fp::memory() {
  static unsigned memory_stalls = 0;
  unsigned char *whatToLoad;
  unsigned whereToLoadFrom;
  unsigned loadableData;
  unsigned currentB; = pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_B];
  instruction_t currentInstruction; = pipeline.stage[EXE_MEM].parsedInstruction;
  opcode_t currentOpcode; = currentInstruction.opcode;
  pipeline.stage[MEM_WB].parsedInstruction = currentInstruction;

  //
  /*paropogate ALU output*/
  /*only if instruction is not LW or SW*/
  unsigned currentALUOutput =
      pipeline.stage[EXE_MEM].spRegisters[EXE_MEM_ALU_OUT];
  if (memory_stalls < data_memory_latency &&
      (currentOpcode == SW || currentOpcode == SWS || currentOpcode == LW ||
       currentOpcode == LWS)) {
    STRUCT_HAZ_MEM_STALL = 1;
    memory_stalls++;
  }

    switch (currentOpcode) {
    case LW:
    case SW:
      memory_stall();
      /* Write to register ahead? */
      // pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;
      break;
    case EOP:
      doMemory = false;
      break;
    default:
      pipeline.stage[MEM_WB].spRegisters[MEM_WB_ALU_OUT] = currentALUOutput;
      doWriteBack = true;
      break;
    }
  // decrement number of memory stages needed
  // Conditionally increment number of write back stages needed
}

  void sim_pipe_fp::instruction_type(opcode_t opcode) {
    switch (opcode) {
    case opcode < LWS:
      return INT_INSTRUCTION;
      break;
    default:
      break;
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

  bool sim_pipe_fp::check_over_write_data_hazard(unsigned dest,
                                                 exe_unit_t type) {}
  bool sim_pipe_fp::check_read_after_write_hazard() {
    instruction_t dec_inst = pipeline.stage[IF_ID].parsedInstruction;
    instruction_t mem_inst = pipeline.stage[EXE_MEM].parsedInstruction;
    instruction_t write_inst = pipeline.stage[MEM_WB].parsedInstruction;
    unsigned decodeSrc1 = dec_inst.src1;
    unsigned decodeSrc2 = dec_inst.src2;
    if (dec_inst.opcode != NOP && dec_inst.opcode != EOP) {
      if (src_dep_check(dec_inst, mem_inst) ||
          src_dep_check(instruction_t dep_inst, instruction_t wb_inst)) {
        if ((instruction_type(mem_inst.opcode) == FP_INSTRUCTIONS) &&
            (fp_reg_dependent(dec_inst))) {
          if ((instruction_type(mem_inst.opcode) == INT_INSTRUCTION) &&
              (instruction_type(dec_inst.opcode) == INT_INSTRUCTION ||
               is_branch(dec_inst.opcode) || dec_inst.opcode == LWS ||
               dec_inst.opcode == SWS)) {
            DATA_HAZ_LATE_READ_STALL = 1;
          }
        }
      }
    }
    // if
  }

  execution_unit::execution_unit(unsigned thisLatency, pipeline_alu thisAlu) {
    reset_unit();
    latency = thisLatency + 1;
    number_cycles = 0;
    executionALU = thisAlu;
  }

  void execution_unit::run_unit() {
    if (number_cycles < latency) {
      number_cycles++;
    }
  }

  bool execution_unit::unit_done() { return number_cycles >= latency; }

  void execution_unit::reset_unit() {
    executionALU.instruction.immediate = UNDEFINED;
    executionALU.instruction.label = "";
    executionALU.aluB = UNDEFINED;
    executionALU.cond = UNDEFINED;
    executionALU.out = UNDEFINED;
    executionALU.instruction.opcode = NOP;
    executionALU.instruction.src1 = UNDEFINED;
    executionALU.instruction.src2 = UNDEFINED;
    executionALU.instruction.dest = UNDEFINED;
  }

  pipeline_alu execution_unit::return_content() { return executionALU; }

  unsigned execution_unit::cycles_left() { return latency - number_cycles; }

  void execution_unit::print_unit() {
    printf("\n");
    printf("EXECUTION UNIT:\n");
    printf("Unit opcode: %d\n", executionALU.instruction.opcode);
    printf("Unit src1:          %d\n", executionALU.instruction.src1);
    printf("unit src2:          %d\n", executionALU.instruction.src2);
    printf("unit dest:           %d\n", executionALU.instruction.dest);
    printf("unit imm:            %d\n", executionALU.instruction.immediate);
    printf("unit b:              %d\n", executionALU.aluB);
    printf("unit latency:        %d\n", latency);
    printf("unit cycles     %d\n", number_cycles);
    printf("------------------------------\n");
  }
