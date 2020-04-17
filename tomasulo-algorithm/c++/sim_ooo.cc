#include "sim_ooo.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

// #define TEST_NUM_CYCLES 4
// #define PRINT_EXECUTION_UNITS 0
// #define EARLY_COMMIT_DEBUG
// #define REG_DBG
// #define EXECUTE_DBG
// #define IMM_DBG
// #define BRANCH_EOP_DBG
//#define SELF_NAMING_DBG
//#define XOR_DBG
//#define PRINT_DBG
//#define CONFLICT_DBG
//#define BRANCH_DBG
//#define STORE_DBG
//#define CASE_8_DBG
//#define ROB_DBG
using namespace std;
//global variable to access instruction memory in issue stage
int instruction_index = 0;
unsigned clock_cycle = 0;
unsigned branchPredictedValue = UNDEFINED;
unsigned branchCorrectedValue = UNDEFINED;
bool programComplete = false;
bool eop_issued = false;
bool branchTrigger = false;
bool branchTriggerCleared = false;
bool doBranchCommit = false;
bool storeCommitLock = false;
bool branchRobCorrect = false;
unsigned storeCommitTime = UNDEFINED;
unsigned storeLatency = UNDEFINED;
unsigned branchDestination = UNDEFINED;
unsigned branchValue = UNDEFINED;
unsigned eop_q_number = UNDEFINED;
unsigned storeExecutionUnitNumber = UNDEFINED;
unsigned storeValue = UNDEFINED;
unsigned storeDestination = UNDEFINED;

// used for debugging purposes
static const char *stage_names[NUM_STAGES] = {"ISSUE", "EXE", "WR", "COMMIT"};
static const char *instr_names[NUM_OPCODES] = {
    "LW",   "SW",  "ADD",  "ADDI", "SUB",  "SUBI", "XOR",   "AND",
    "MULT", "DIV", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ",  "BGEZ",
    "JUMP", "EOP", "LWS",  "SWS",  "ADDS", "SUBS", "MULTS", "DIVS"};
static const char *res_station_names[5] = {"Int", "Add", "Mult", "Load"};

/* =============================================================

   HELPER FUNCTIONS (misc)

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

/* the following six functions return the kind of the considered opcdoe */

bool is_branch(opcode_t opcode) {
  return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ ||
          opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode) {
  return (opcode == LW || opcode == SW || opcode == LWS || opcode == SWS);
}

bool is_int_r(opcode_t opcode) {
  return (opcode == ADD || opcode == SUB || opcode == XOR || opcode == AND);
}

bool is_int_imm(opcode_t opcode) { return (opcode == ADDI || opcode == SUBI); }

bool is_int(opcode_t opcode) {
  return (is_int_r(opcode) || is_int_imm(opcode));
}

bool is_fp_alu(opcode_t opcode) {
  return (opcode == ADDS || opcode == SUBS || opcode == MULTS ||
          opcode == DIVS);
}

bool sim_ooo::is_load_instruction(opcode_t thisOpcode){
  return (thisOpcode == LWS || thisOpcode == LW);
}

bool sim_ooo::is_store_instruction(opcode_t thisOpcode){
  return (thisOpcode == SW || thisOpcode == SWS);
}

/* clears a ROB entry */
void clean_rob(rob_entry_t *entry) {
  entry->ready = false;
  entry->pc = UNDEFINED;
  entry->state = ISSUE;
  entry->destination = UNDEFINED;
  entry->value = UNDEFINED;
  entry->empty=true;
}

/* clears a reservation station */
void clean_res_station(res_station_entry_t *entry) {
  entry->pc = UNDEFINED;
  entry->value1 = UNDEFINED;
  entry->value2 = UNDEFINED;
  entry->tag1 = UNDEFINED;
  entry->tag2 = UNDEFINED;
  entry->destination = UNDEFINED;
  entry->address = UNDEFINED;
}

/* clears an entry if the instruction window */
void clean_instr_window(instr_window_entry_t *entry) {
  entry->pc = UNDEFINED;
  entry->issue = UNDEFINED;
  entry->exe = UNDEFINED;
  entry->wr = UNDEFINED;
  entry->commit = UNDEFINED;
}

/* implements the ALU operation
   NOTE: this function does not cover LOADS and STORES!
*/
unsigned alu(opcode_t opcode, unsigned value1, unsigned value2,
             unsigned immediate, unsigned pc) {
  unsigned result;
  switch (opcode) {
  case ADD:
  case ADDI:
    result = value1 + value2;
    break;
  case SUB:
  case SUBI:
    result = value1 - value2;
    break;
  case XOR:
    result = value1 ^ value2;
    break;
  case AND:
    result = value1 & value2;
    break;
  case MULT:
    result = value1 * value2;
    break;
  case DIV:
    result = value1 / value2;
    break;
  case ADDS:
    result = float2unsigned(unsigned2float(value1) + unsigned2float(value2));
    break;
  case SUBS:
    result = float2unsigned(unsigned2float(value1) - unsigned2float(value2));
    break;
  case MULTS:
    result = float2unsigned(unsigned2float(value1) * unsigned2float(value2));
    break;
  case DIVS:
    result = float2unsigned(unsigned2float(value1) / unsigned2float(value2));
    break;
  case JUMP:
    result = pc + 4 + immediate;
    break;
  default: // branches
    int reg = (int)value1;
    bool condition =
        ((opcode == BEQZ && reg == 0) || (opcode == BNEZ && reg != 0) ||
         (opcode == BGEZ && reg >= 0) || (opcode == BLEZ && reg <= 0) ||
         (opcode == BGTZ && reg > 0) || (opcode == BLTZ && reg < 0));
    if (condition)
      result = pc + 4 + immediate;
    else
      result = pc + 4;
    break;
  }
  return result;
}

/* writes the data memory at the specified address */
void sim_ooo::write_memory(unsigned address, unsigned value) {
  unsigned2char(value, data_memory + address);
}

/* =============================================================

   Handling of FUNCTIONAL UNITS

   ============================================================= */

/* initializes an execution unit */
void sim_ooo::init_exec_unit(exe_unit_t exec_unit, unsigned latency,
                             unsigned instances) {
  for (unsigned i = 0; i < instances; i++) {
    exec_units[num_units].type = exec_unit;
    exec_units[num_units].latency = latency;
    exec_units[num_units].busy = 0;
    exec_units[num_units].pc = UNDEFINED;
    num_units++;
  }
}

/* returns a free unit for that particular operation or UNDEFINED if no unit is
 * currently available */
unsigned sim_ooo::get_free_unit(opcode_t opcode) {
  if (num_units == 0) {
    cout << "ERROR:: simulator does not have any execution units!\n";
    exit(-1);
  }
  for (unsigned u = 0; u < num_units; u++) {
    switch (opcode) {
    // Integer unit
    case ADD:
    case ADDI:
    case SUB:
    case SUBI:
    case XOR:
    case AND:
    case BEQZ:
    case BNEZ:
    case BLTZ:
    case BGTZ:
    case BLEZ:
    case BGEZ:
    case JUMP:
      if (exec_units[u].type == INTEGER && exec_units[u].busy == 0 &&
          exec_units[u].pc == UNDEFINED)
        return u;
      break;
    // memory unit
    case LW:
    case SW:
    case LWS:
    case SWS:
      if (exec_units[u].type == MEMORY && exec_units[u].busy == 0 &&
          exec_units[u].pc == UNDEFINED)
        return u;
      break;
    // FP adder
    case ADDS:
    case SUBS:
      if (exec_units[u].type == ADDER && exec_units[u].busy == 0 &&
          exec_units[u].pc == UNDEFINED)
        return u;
      break;
    // Multiplier
    case MULT:
    case MULTS:
      if (exec_units[u].type == MULTIPLIER && exec_units[u].busy == 0 &&
          exec_units[u].pc == UNDEFINED)
        return u;
      break;
    // Divider
    case DIV:
    case DIVS:
      if (exec_units[u].type == DIVIDER && exec_units[u].busy == 0 &&
          exec_units[u].pc == UNDEFINED)
        return u;
      break;
    default:
      cout << "ERROR:: operations not requiring exec unit!\n";
      exit(-1);
    }
  }
  return UNDEFINED;
}

/* ============================================================================

   Primitives used to print out the state of each component of the processor:
        - registers
        - data memory
        - instruction window
                - reservation stations and load buffers
                - (cycle-by-cycle) execution log
        - execution statistics (CPI, # instructions executed, # clock cycles)

   ===========================================================================
 */

/* prints the content of the data memory */
void sim_ooo::print_memory(unsigned start_address, unsigned end_address) {
  cout << "DATA MEMORY[0x" << hex << setw(8) << setfill('0') << start_address
       << ":0x" << hex << setw(8) << setfill('0') << end_address << "]" << endl;
  for (unsigned i = start_address; i < end_address; i++) {
    if (i % 4 == 0)
      cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
    cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
    if (i % 4 == 3) {
      cout << endl;
    }
  }
}

/* prints the value of the registers */
void sim_ooo::print_registers() {
  unsigned i;
  cout << "GENERAL PURPOSE REGISTERS" << endl;
  cout << setfill(' ') << setw(8) << "Register" << setw(22) << "Value"
       << setw(5) << "ROB" << endl;
  for (i = 0; i < NUM_GP_REGISTERS; i++) {
    if (get_int_register_tag(i) != UNDEFINED)
      cout << setfill(' ') << setw(7) << "R" << dec << i << setw(22) << "-"
           << setw(5) << get_int_register_tag(i) << endl;
    else if (get_int_register(i) != (int)UNDEFINED)
      cout << setfill(' ') << setw(7) << "R" << dec << i << setw(11)
           << get_int_register(i) << hex << "/0x" << setw(8) << setfill('0')
           << get_int_register(i) << setfill(' ') << setw(5) << "-" << endl;
  }
  for (i = 0; i < NUM_GP_REGISTERS; i++) {
    if (get_fp_register_tag(i) != UNDEFINED)
      cout << setfill(' ') << setw(7) << "F" << dec << i << setw(22) << "-"
           << setw(5) << get_fp_register_tag(i) << endl;
    else if (get_fp_register(i) != UNDEFINED)
      cout << setfill(' ') << setw(7) << "F" << dec << i << setw(11)
           << get_fp_register(i) << hex << "/0x" << setw(8) << setfill('0')
           << float2unsigned(get_fp_register(i)) << setfill(' ') << setw(5)
           << "-" << endl;
  }
  cout << endl;
}

/* prints the content of the ROB */
void sim_ooo::print_rob() {
  cout << "REORDER BUFFER" << endl;
  cout << setfill(' ') << setw(5) << "Entry" << setw(6) << "Busy" << setw(7)
       << "Ready" << setw(12) << "PC" << setw(10) << "State" << setw(6)
       << "Dest" << setw(12) << "Value" << endl;
  for (unsigned i = 0; i < rob.num_entries; i++) {
    rob_entry_t entry = rob.entries[i];
    instruction_t instruction;
    if (entry.pc != UNDEFINED)
      instruction = instr_memory[(entry.pc - instr_base_address) >> 2];
    cout << setfill(' ');
    cout << setw(5) << i;
    cout << setw(6);
    if (entry.pc == UNDEFINED)
      cout << "no";
    else
      cout << "yes";
    cout << setw(7);
    if (entry.ready)
      cout << "yes";
    else
      cout << "no";
    if (entry.pc != UNDEFINED)
      cout << "  0x" << hex << setfill('0') << setw(8) << entry.pc;
    else
      cout << setw(12) << "-";
    cout << setfill(' ') << setw(10);
    if (entry.pc == UNDEFINED)
      cout << "-";
    else
      cout << stage_names[entry.state];
    if (entry.destination == UNDEFINED)
      cout << setw(6) << "-";
    else {
      if (instruction.opcode == SW || instruction.opcode == SWS)
        cout << setw(6) << dec << entry.destination;
      else if (entry.destination < NUM_GP_REGISTERS)
        cout << setw(5) << "R" << dec << entry.destination;
      else
        cout << setw(5) << "F" << dec << entry.destination - NUM_GP_REGISTERS;
    }
    if (entry.value != UNDEFINED)
      cout << "  0x" << hex << setw(8) << setfill('0') << entry.value << endl;
    else
      cout << setw(12) << setfill(' ') << "-" << endl;
  }
  cout << endl;
}

/* prints the content of the reservation stations */
void sim_ooo::print_reservation_stations() {
  cout << "RESERVATION STATIONS" << endl;
  cout << setfill(' ');
  cout << setw(7) << "Name" << setw(6) << "Busy" << setw(12) << "PC" << setw(12)
       << "Vj" << setw(12) << "Vk" << setw(6) << "Qj" << setw(6) << "Qk"
       << setw(6) << "Dest" << setw(12) << "Address" << endl;
  for (unsigned i = 0; i < reservation_stations.num_entries; i++) {
    res_station_entry_t entry = reservation_stations.entries[i];
    cout << setfill(' ');
    cout << setw(6);
    cout << res_station_names[entry.type];
    cout << entry.name + 1;
    cout << setw(6);
    if (entry.pc == UNDEFINED)
      cout << "no";
    else
      cout << "yes";
    if (entry.pc != UNDEFINED)
      cout << setw(4) << "  0x" << hex << setfill('0') << setw(8) << entry.pc;
    else
      cout << setfill(' ') << setw(12) << "-";
    if (entry.value1 != UNDEFINED)
      cout << "  0x" << setfill('0') << setw(8) << hex << entry.value1;
    else
      cout << setfill(' ') << setw(12) << "-";
    if (entry.value2 != UNDEFINED)
      cout << "  0x" << setfill('0') << setw(8) << hex << entry.value2;
    else
      cout << setfill(' ') << setw(12) << "-";
    cout << setfill(' ');
    cout << setw(6);
    if (entry.tag1 != UNDEFINED)
      cout << dec << entry.tag1;
    else
      cout << "-";
    cout << setw(6);
    if (entry.tag2 != UNDEFINED)
      cout << dec << entry.tag2;
    else
      cout << "-";
    cout << setw(6);
    if (entry.destination != UNDEFINED)
      cout << dec << entry.destination;
    else
      cout << "-";
    if (entry.address != UNDEFINED)
      cout << setw(4) << "  0x" << setfill('0') << setw(8) << hex
           << entry.address;
    else
      cout << setfill(' ') << setw(12) << "-";
    cout << endl;
  }
  cout << endl;
}

/* prints the state of the pending instructions */
void sim_ooo::print_pending_instructions() {
  cout << "PENDING INSTRUCTIONS STATUS" << endl;
  cout << setfill(' ');
  cout << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7)
       << "WR" << setw(7) << "Commit";
  cout << endl;
  for (unsigned i = 0; i < pending_instructions.num_entries; i++) {
    instr_window_entry_t entry = pending_instructions.entries[i];
    if (entry.pc != UNDEFINED)
      cout << "0x" << setfill('0') << setw(8) << hex << entry.pc;
    else
      cout << setfill(' ') << setw(10) << "-";
    cout << setfill(' ');
    cout << setw(7);
    if (entry.issue != UNDEFINED)
      cout << dec << entry.issue;
    else
      cout << "-";
    cout << setw(7);
    if (entry.exe != UNDEFINED)
      cout << dec << entry.exe;
    else
      cout << "-";
    cout << setw(7);
    if (entry.wr != UNDEFINED)
      cout << dec << entry.wr;
    else
      cout << "-";
    cout << setw(7);
    if (entry.commit != UNDEFINED)
      cout << dec << entry.commit;
    else
      cout << "-";
    cout << endl;
  }
  cout << endl;
}

/* initializes the execution log */
void sim_ooo::init_log() {
  log << "EXECUTION LOG" << endl;
  log << setfill(' ');
  log << setw(10) << "PC" << setw(7) << "Issue" << setw(7) << "Exe" << setw(7)
      << "WR" << setw(7) << "Commit";
  log << endl;
}

/* adds an instruction to the log */
void sim_ooo::commit_to_log(instr_window_entry_t entry) {
  if (entry.pc != UNDEFINED)
    log << "0x" << setfill('0') << setw(8) << hex << entry.pc;
  else
    log << setfill(' ') << setw(10) << "-";
  log << setfill(' ');
  log << setw(7);
  if (entry.issue != UNDEFINED)
    log << dec << entry.issue;
  else
    log << "-";
  log << setw(7);
  if (entry.exe != UNDEFINED)
    log << dec << entry.exe;
  else
    log << "-";
  log << setw(7);
  if (entry.wr != UNDEFINED)
    log << dec << entry.wr;
  else
    log << "-";
  log << setw(7);
  if (entry.commit != UNDEFINED)
    log << dec << entry.commit;
  else
    log << "-";
  log << endl;
}

/* prints the content of the log */
void sim_ooo::print_log() { cout << log.str(); }

/* prints the state of the pending instruction, the content of the ROB, the
 * content of the reservation stations and of the registers */
void sim_ooo::print_status() {
  print_pending_instructions();
  print_rob();
  print_reservation_stations();
  print_registers();
}

/* execution statistics */

float sim_ooo::get_IPC() { return (float)instructions_executed / (clock_cycle-1); }

unsigned sim_ooo::get_instructions_executed() { return instructions_executed; }

unsigned sim_ooo::get_clock_cycles() { return clock_cycle-1; }

/* ============================================================================

   PARSER

   ===========================================================================
 */

void sim_ooo::load_program(const char *filename, unsigned base_address) {

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
    case AND:
    case MULT:
    case DIV:
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
}

/* ============================================================================

   Simulator creation, initialization and deallocation

   ===========================================================================
 */

sim_ooo::sim_ooo(unsigned mem_size, unsigned rob_size,
                 unsigned num_int_res_stations, unsigned num_add_res_stations,
                 unsigned num_mul_res_stations, unsigned num_load_res_stations,
                 unsigned max_issue) {
  // memory
  data_memory_size = mem_size;
  data_memory = new unsigned char[data_memory_size];

  // issue width
  issue_width = max_issue;

  // rob, instruction window, reservation stations
  rob.num_entries = rob_size;
  pending_instructions.num_entries = rob_size;
  reservation_stations.num_entries =
      num_int_res_stations + num_load_res_stations + num_add_res_stations +
      num_mul_res_stations;
  rob.entries = new rob_entry_t[rob_size];
  pending_instructions.entries = new instr_window_entry_t[rob_size];
  reservation_stations.entries =
      new res_station_entry_t[reservation_stations.num_entries];
  unsigned n = 0;
  for (unsigned i = 0; i < num_int_res_stations; i++, n++) {
    reservation_stations.entries[n].type = INTEGER_RS;
    reservation_stations.entries[n].name = i;
  }
  for (unsigned i = 0; i < num_load_res_stations; i++, n++) {
    reservation_stations.entries[n].type = LOAD_B;
    reservation_stations.entries[n].name = i;
  }
  for (unsigned i = 0; i < num_add_res_stations; i++, n++) {
    reservation_stations.entries[n].type = ADD_RS;
    reservation_stations.entries[n].name = i;
  }
  for (unsigned i = 0; i < num_mul_res_stations; i++, n++) {
    reservation_stations.entries[n].type = MULT_RS;
    reservation_stations.entries[n].name = i;
  }
  // execution units
  num_units = 0;
  reset();
}

sim_ooo::~sim_ooo() {
  delete[] data_memory;
  delete[] rob.entries;
  delete[] pending_instructions.entries;
  delete[] reservation_stations.entries;
}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */

/* core of the simulator */
//////////////////////
// RUN FUNCTIONS    //
//////////////////////

void sim_ooo::run(unsigned cycles) {
  if(clock_cycle == 0){
//    create_res_stations_stats();
  }
  unsigned cyclesRan = 0;
  if(cycles){
    while(cyclesRan<cycles){
      a_cycle();
      cyclesRan++;
      clock_cycle++;
    }
  }
  else{
    #ifdef CASE_8_DBG
    while((!programComplete)&&(clock_cycle<100)){
      a_cycle();
      clock_cycle++;
    }
    #endif
    #ifndef CASE_8_DBG
    while(!programComplete){
      a_cycle();
      clock_cycle++;
    }
    #endif
  }
}

void sim_ooo::a_cycle(){
  if(!storeCommitLock){
    commit();
  }
  else if(storeCommitLock){
    store_commit();
  }
  write_results();
  execute();
  for(unsigned i = 0; i< issue_width; i++){
    #ifdef ISSUE_DBG
    cout << "Attempting issue # "<< i+1 << endl;
    #endif
    if (!eop_issued)
      issue_instruction();
  }
  post_process();
}


// reset the state of the simulator - please complete
void sim_ooo::reset() {

  // init instruction log
  init_log();

  // data memory
  for (unsigned i = 0; i < data_memory_size; i++)
    data_memory[i] = 0xff;

  // instr memory
  for (int i = 0; i < PROGRAM_SIZE; i++) {
    instr_memory[i].opcode = (opcode_t)EOP;
    instr_memory[i].src1 = UNDEFINED;
    instr_memory[i].src2 = UNDEFINED;
    instr_memory[i].dest = UNDEFINED;
    instr_memory[i].immediate = UNDEFINED;
  }

  // general purpose registers
  for (int i = 0; i < NUM_GP_REGISTERS; i++) {
    int_gp[i].value=UNDEFINED;
    int_gp[i].name = UNDEFINED;
    int_gp[i].busy = false;
    float_gp[i].value = UNDEFINED;
    float_gp[i].name = UNDEFINED;
    float_gp[i].busy = false;
  }

  // pending_instructions
  for (unsigned i = 0; i < pending_instructions.num_entries; i++) {
    clean_instr_window(&pending_instructions.entries[i]);
  }

  // rob
  for (unsigned i = 0; i < rob.num_entries; i++) {
    clean_rob(&rob.entries[i]);
  }
  // cleaning reservation stations

  for (unsigned i = 0; i < reservation_stations.num_entries; i++) {
    clean_res_station(&reservation_stations.entries[i]);
  }

  // execution statistics
  clock_cycle = 0;
  instructions_executed = 0;

  // other required initializations
}

 ////////////////////////
 // REGISTERS RELATED  //
 ////////////////////////

int sim_ooo::get_int_register(unsigned reg) {
  return int_gp[reg].value; // please modify
}

void sim_ooo::set_int_register(unsigned reg, int value) {
  int_gp[reg].value = value;
}

float sim_ooo::get_fp_register(unsigned reg) {
  return float_gp[reg].value; // please modify
}

void sim_ooo::set_fp_register(unsigned reg, float value) {
  float_gp[reg].value = value;
}

unsigned sim_ooo::get_int_register_tag(unsigned reg) {
  /*compare rob destination column with 'reg argument'*/
  /*loop through entries in rob*/
  return int_gp[reg].name;
}

unsigned sim_ooo::get_fp_register_tag(unsigned reg) {
  return float_gp[reg].name; // please modify
}

// set float register entry to null
// change some float register field value
void float_reg_set(float_gp_reg_entry &thisEntry, float thisValue = 0xff,
                   unsigned thisName = UNDEFINED, bool toggle = false) {
  if (thisValue != 0xff) {
    thisEntry.value = thisValue;
  }
  if (toggle) {
    thisEntry.busy = !(thisEntry.busy);
  }
  if (thisName != UNDEFINED) {
    thisEntry.name = thisName;
  }
}

// change some int register field value
void int_reg_set(float_gp_reg_entry &thisEntry, int thisValue = 0xff,
                 unsigned thisName = UNDEFINED, bool toggle = false) {
  if (thisValue != 0xff) {
    thisEntry.value = thisValue;
  }
  if (toggle) {
    thisEntry.busy = !(thisEntry.busy);
  }
  if (thisName != UNDEFINED) {
    thisEntry.name = thisName;
  }
}

/////////////////////
// ISSUE FUNCTIONS //
/////////////////////
// function to handle issue stage of processor
void sim_ooo::issue_instruction(){
  static unsigned instructionIndex = instr_base_address;
  static unsigned robEntryNumber;
  static unsigned resStationIndex = UNDEFINED;
  static bool stallAtIssue = false;
  static bool res_station_full_stall = false;
  static bool rob_buffer_full_stall = false;
  static instruction_t previousInstruction;
  if(branchTriggerCleared){
#ifdef BRANCH_EOP_DBG
    cout << "Swapping to corrected value, index " << branchCorrectedValue << endl;
#endif
    instructionIndex = branchCorrectedValue;
    branchTriggerCleared = false;
    branchTrigger = false;
  }
  instruction_t currentInstruction = instr_memory[instructionIndex];
  //check if rob and reservation station is full
#ifdef PRINT_DBG
  cout << instructionIndex << endl;;
  print_string_opcode(currentInstruction.opcode);
#endif
  if(!rob_full()){
#ifdef PRINT_DBG
    cout << "****ISSUING INSTRUCTION****" << endl;
    cout << "->Rob is not full" << endl;
#endif
    res_station_t thisResStationType;
    if (currentInstruction.opcode == EOP) {
      eop_issued=true;
#ifdef PRINT_DBG
      cout << "EOP triggered" << endl;
      print_rob();
#endif
      return;
    }
    thisResStationType = get_station_type(currentInstruction);
    resStationIndex = get_available_res_station(thisResStationType);
    if(resStationIndex!=UNDEFINED){
      //creat instruction map entry
      map_entry_t newSet;
      //set instruction map entry to pc (not instruction_memory index)
      unsigned keyValue = instructionIndex*4;

#ifdef PRINT_DBG
      cout << "->Res stations are not full" << endl;
#endif
      // if not full, add instruction
      robEntryNumber = rob_add(currentInstruction, keyValue);

      #ifdef PRINT_DBG
      print_string_opcode(currentInstruction.opcode);
      #endif
      // add rob entry to queue

      robq.push(robEntryNumber);
      if(branchTrigger){
#ifdef BRANCH_DBG
        cout << "Branch trigger is true, tagging this instruction" << endl;
#endif
        branch_instruction_map_keys.push(keyValue);
      }
      if(is_memory(currentInstruction.opcode)){
        memory_instruction_q.push(keyValue);
        #ifdef STORE_DBG
        cout << "Adding instruction to memory q" << endl;
        #endif
      }
      newSet.instrMemoryIndex = instructionIndex;
      newSet.robIndex=robEntryNumber;
      //add res station index number to set
      newSet.resStationIndex=resStationIndex;
      //add to instruction window and set
      instruction_window_add(robEntryNumber, keyValue);

      newSet.instrWindowIndex = robEntryNumber;
      //add opcode
      newSet.instructionOpcode = currentInstruction.opcode;
      //set ready to write to false
      newSet.readyToWrite = false;
      //set ready to commit to false
      newSet.readyToCommit = false;
      //set map entry's execution unis to undefined
      newSet.executionUnitNumber = UNDEFINED;
      //set map entries station type
      newSet.resStationType = thisResStationType;
      //time that write values will be available
      newSet.valuesAvailable = 0;
      //time that res station will be available
      newSet.resStationAvailable = UNDEFINED;
      //add set to map
      instruction_map.insert(make_pair(keyValue, newSet));
      //add to reservation station
      map_entry_t newMapEntry = instruction_map.find(keyValue)->second;
      #ifdef PRINT_DBG
//print reservation station info
      cout << "->Reservation station info:" << endl;
      #endif
      reservation_station_add(newMapEntry, keyValue);
      //add rob index number to set
      // increment pc
      instructionIndex = instructionIndex + 1;
      //set previous instruction marker
      if (is_branch(currentInstruction.opcode)) {
#ifdef BRANCH_EOP_DBG
        cout << "Branch triggered" << endl;
        cout << "Branch predicted not taken" << endl;
        cout << "Branch predicted instruction: ";
        cout << instructionIndex*4 << endl;
        #endif
        branchPredictedValue = instructionIndex*4;
        branchTrigger = true;
      }
      previousInstruction = currentInstruction;
      //set current instruction for next iteration of this function
      res_station_full_stall = false;
      stallAtIssue = false;
    }
    else{
      #ifdef PRINT_DBG
      cout << "[X] Res stations are full" << endl;
      #endif
      res_station_full_stall = true;
    }
  }
  else {
      #ifdef PRINT_DBG
    cout << "[X] Rob is full" << endl;
      #endif
    stallAtIssue = true;
    previousInstruction = currentInstruction;
}
        #ifdef PRINT_DBG
  cout << "****DONE ISSUING INSTRUCTION****" << endl;
  cout << "Clock cycle: " << clock_cycle << endl;
#endif//  currentInstruction = instr_memory[instruction_index];
}

unsigned sim_ooo::mem_to_index(unsigned thisMemoryValue){
  return (thisMemoryValue - 0x1000000)/4;

}
  //decode instruction in issue stage
  //pass values to reservation stations
void sim_ooo::station_delay_check(res_station_t thisReservationStation){
  std::map<unsigned, map_entry_t>::iterator it;
  it = instruction_map.begin();
  while(it!=instruction_map.end()){
    if(it->second.resStationType==thisReservationStation){
      if(it->second.resStationAvailable==UNDEFINED){
#ifdef PRINT_DBG
        cout << "Res. station not available, available time not defined yet." << endl;
        #endif
      }
      if(it->second.resStationAvailable>clock_cycle){
      #ifdef PRINT_DBG
        cout << "Res. station not available until: " << it->second.resStationAvailable<< endl;
      #endif
        return;
      }
    }
    it++;
  }

}

void sim_ooo::check_self_naming(){
  unsigned resStationIndex;
  unsigned resStationDestination;
  unsigned robIndex;
  unsigned robDestination;
  unsigned thisRegister;
  unsigned pcOfInstruction;
  unsigned instructionIndex;
  bool fp = false;
  instruction_t thisInstruction;
  std::map<unsigned, map_entry_t>::iterator it;
  it = instruction_map.begin();
  while(it!=instruction_map.end()){
    resStationIndex = it->second.resStationIndex;
    robIndex = it->second.robIndex;
    resStationDestination = reservation_stations.entries[resStationIndex].destination;
    robDestination = rob.entries[robIndex].destination;
    pcOfInstruction = rob.entries[robIndex].pc;
    instructionIndex = pcOfInstruction/4;
    thisInstruction = instr_memory[instructionIndex];
    if(robDestination>32){
      robDestination-=32;
      fp = true;
    }
#ifdef SELF_NAMING_DBG
    cout << "Register index: " << robDestination << endl;
    cout << "Value in this register: " << int_gp[robDestination].value << endl;
      #endif
    if(thisInstruction.src1==robDestination){
#ifdef SELF_NAMING_DBG
      cout << "At res. station " << resStationIndex << endl;
      cout << "Self naming conflict on source 1" << endl;
      cout << "Swapping tag for value" << endl;
      #endif
      if(fp){
        reservation_stations.entries[resStationIndex].value1 = float_gp[robDestination].value;
      }
      else{
        reservation_stations.entries[resStationIndex].value1 = int_gp[robDestination].value;
      }
        reservation_stations.entries[resStationIndex].tag1= UNDEFINED;
    }
    if(thisInstruction.src2==robDestination){
#ifdef SELF_NAMING_DBG
      cout << "Self naming conflict on source 2" << endl;
      cout << "Swapping tag for value" << endl;
      #endif
      if(fp){
        reservation_stations.entries[resStationIndex].value2 = float_gp[robDestination].value;
      }
      else{
        reservation_stations.entries[resStationIndex].value2 = int_gp[robDestination].value;
      }
        reservation_stations.entries[resStationIndex].tag2= UNDEFINED;
    }
#ifdef SELF_NAMING_DBG
//    print_reservation_stations();
      #endif

    //if()
    it++;
  }
}

///////////////////
// ROB FUNCTIONS //
///////////////////

//function to determine if there is an empty slot in the reorder buffer
bool sim_ooo::rob_full(){
  for(unsigned i = 0; i < rob.num_entries; i++){
    if(rob.entries[i].empty == true){
      return false;
    }
  }
  return true;
}
//function to add instruction to reorder buffer
unsigned sim_ooo::rob_add(instruction_t &thisInstruction, unsigned thisPC){
  //init rob index to 0;
  static unsigned robIndex = 0;
  unsigned robIndexReturn;
  //init pc to base address

  //get instruction
  //add instruction to rob
  if(robq.empty())
  robIndex = 0;

  rob.entries[robIndex].pc = thisPC;
  rob.entries[robIndex].state = ISSUE;
  //floation point instructions must be passed a value >= 32 to be put in 'F'
  //registers
  if (is_fp_instruction(thisInstruction.opcode)) {
    rob.entries[robIndex].destination = thisInstruction.dest + 32;
#ifdef REG_DBG
    if (thisInstruction.opcode == ADDS) {
      unsigned registerName = thisInstruction.dest;
      cout << "This ADDS destination is: "  << registerName << endl;
      cout << "This ADDS ROB index is: " << robIndex << endl;
      float_gp[registerName].name = robIndex;
      print_registers();
    }
#endif
  }
  // integer instructions need no correction
  else {
    // conditionals don't get destination values in ROB
    if (!is_conditional(thisInstruction.opcode))
      rob.entries[robIndex].destination = thisInstruction.dest;
    else
      rob.entries[robIndex].destination = UNDEFINED;
  }
  // this entry is no longer open
  rob.entries[robIndex].empty = false;
  // for sw and sws, destination is not available until execution
  // for now, use immediate value
  if (thisInstruction.opcode == SW || thisInstruction.opcode == SWS) {
    rob.entries[robIndex].destination = UNDEFINED;
  } else {
    //fp instructions go into float registers, update float names
    if (is_fp_instruction(thisInstruction.opcode)) {
      float_gp[thisInstruction.dest].name = robIndex;
#ifdef REG_DBG
      if (thisInstruction.opcode == ADDS) {
        cout << "This target register's name is: "
             << float_gp[thisInstruction.dest].name << endl;
      }
#endif
    } else {
      // int instrutions go into gp registers, if destination exists, write dest
      if (!is_conditional(thisInstruction.opcode)) {
        int_gp[thisInstruction.dest].name = robIndex;
      }
    }
  }
  robIndexReturn = robIndex;
  robIndex++;
  if(robIndex == rob.num_entries){
    robIndex = 0;
  }
#ifdef REG_DBG
  cout << "Printing registers again" << endl;
      print_registers();
#endif

  return robIndexReturn;
}

bool sim_ooo::is_conditional(opcode_t thisOpcode) {
  switch (thisOpcode) {
  case BEQZ:
  case BNEZ:
  case BLTZ:
  case BGTZ:
  case BLEZ:
  case BGEZ:
    return true;
    break;
  default:
    return false;
  }
}

//////////////////////////////////////
// RESERVATION STATION FUNCTIONS    //
//////////////////////////////////////

unsigned sim_ooo::pending_int_src_check(unsigned int thisSource){
  if(int_gp[thisSource].name!=UNDEFINED){
    return int_gp[thisSource].name;
  }
  return UNDEFINED;
}

res_station_t sim_ooo::get_station_type(instruction_t thisInstruction){
  switch(thisInstruction.opcode){
  case ADD:
  case SUB:
  case XOR:
  case ADDI:
  case AND:
  case SUBI:
  case MULT:
  case DIV:
  case BEQZ:
  case BNEZ:
  case BLTZ:
  case BGTZ:
  case BLEZ:
  case BGEZ:
  case JUMP:
  case EOP:
    return INTEGER_RS;
    break;
  case MULTS:
  case DIVS:
    return MULT_RS;
    break;
  case LW:
  case LWS:
  case SW:
  case SWS:
    return LOAD_B;
    break;
  case ADDS:
  case SUBS:
    return ADD_RS;
    break;
  }
}

unsigned sim_ooo::get_available_res_station(res_station_t thisTypeOfStation){
  for(unsigned i = 0; i< reservation_stations.num_entries; i++){
    if((reservation_stations.entries[i].type == thisTypeOfStation) && (reservation_stations.entries[i].pc==UNDEFINED)){
      return i;
    }
  }
  return UNDEFINED;

}

void sim_ooo::reservation_station_add(map_entry_t thisMapEntry, unsigned pcOfInstruction){
  unsigned setTag1 = UNDEFINED;
  unsigned setTag2 = UNDEFINED;
  unsigned robEntry = UNDEFINED;

  bool intReg = is_int_r(thisMapEntry.instructionOpcode);
  bool fpReg = is_fp_alu(thisMapEntry.instructionOpcode);

  unsigned thisStationNumber = thisMapEntry.resStationIndex;
  reservation_stations.entries[thisStationNumber].pc = pcOfInstruction;
  reservation_stations.entries[thisStationNumber].destination = thisMapEntry.robIndex;
  //value or tag for appropriate arguments
  //following is for two sources, one destination
  if(intReg||fpReg){
    two_argument_tag_check(thisMapEntry);
  }
  //follwoing is for one source, one destination, and immediate
  else if(is_int_imm(thisMapEntry.instructionOpcode)){
    single_argument_tag_check(thisMapEntry);
  }
  else if(is_conditional(thisMapEntry.instructionOpcode)){
#ifdef PRINT_DBG
    print_pc_to_instruction(pcOfInstruction);
  #endif
    conditional_argument_tag_check(thisMapEntry);
  }
  //following is for load and store instructions
  else if(is_load_instruction(thisMapEntry.instructionOpcode)){
    load_argument_tag_check(thisMapEntry);
  }
  else if (is_store_instruction(thisMapEntry.instructionOpcode)){
    store_argument_tag_check(thisMapEntry);
  }

//  check_self_naming();

}

void sim_ooo::two_argument_tag_check(map_entry_t thisMapEntry){
  unsigned setTag1 = UNDEFINED;
  unsigned setTag2 = UNDEFINED;
  unsigned setVal1 = UNDEFINED;
  unsigned setVal2 = UNDEFINED;
  unsigned thisStationNumber = thisMapEntry.resStationIndex;
  unsigned thisRobIndex = thisMapEntry.robIndex;
  unsigned source1 = instr_memory[thisMapEntry.instrMemoryIndex].src1;
  unsigned source2 = instr_memory[thisMapEntry.instrMemoryIndex].src2;
  bool fpReg = is_fp_alu(thisMapEntry.instructionOpcode);
  bool waitOnValue = false;
#ifdef REG_DBG
  #endif
  reservation_stations.entries[thisStationNumber].address=UNDEFINED;
  #ifdef PRINT_DBG
  print_pc_to_instruction(reservation_stations.entries[thisMapEntry.resStationIndex].pc);
  #endif
unsigned pcForMap = reservation_stations.entries[thisStationNumber].pc;
  if (fpReg) {
    // check fp registers
    source1 = source1 + 32;
    source2 = source2 + 32;
  }
  #ifdef CONFLICT_DBG
    if(thisMapEntry.instructionOpcode==DIVS){
      cout << " " << setTag1 << endl;
//      print_rob();
    }
  #endif

  setVal1 = find_value_in_rob(source1);
  setVal2 = find_value_in_rob(source2);

  if(setVal1 == UNDEFINED){
    setTag1 = find_tag_in_rob(source1,thisRobIndex);
  #ifdef CONFLICT_DBG
    if(thisMapEntry.instructionOpcode==DIVS){
      cout << "Tag returned: " << setTag1 << endl;
//      print_rob();
    }
  #endif
  }

  if(setVal2==UNDEFINED){
    setTag2 = find_tag_in_rob(source2, thisRobIndex);
  }

  if(setVal1 != UNDEFINED){
      reservation_stations.entries[thisStationNumber].value1 = setVal1;
      reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
  }

  else if(setTag1!=UNDEFINED){
      reservation_stations.entries[thisStationNumber].value1 = UNDEFINED;
      reservation_stations.entries[thisStationNumber].tag1 = setTag1;
      waitOnValue = true;
  }

  else if(setTag1==UNDEFINED && setVal1==UNDEFINED){
    reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
    if(fpReg){
     source1 = source1 - 32;
        reservation_stations.entries[thisStationNumber].value1=float2unsigned(get_fp_register(source1));
    }
    else{
        reservation_stations.entries[thisStationNumber].value1=get_int_register(source1);
    }

  }
  if (setVal2 != UNDEFINED) {
    // add tag1 to reservation station
    reservation_stations.entries[thisStationNumber].tag2 = UNDEFINED;
    // set value1 to undefined
    reservation_stations.entries[thisStationNumber].value2 = setVal2;
  }
  else if (setTag2 != UNDEFINED) {
      reservation_stations.entries[thisStationNumber].value2 = UNDEFINED;
      reservation_stations.entries[thisStationNumber].tag2 = setTag2;
      waitOnValue = true;
  }
  else if(setTag2==UNDEFINED && setVal2==UNDEFINED){
    reservation_stations.entries[thisStationNumber].tag2 = UNDEFINED;
    if(fpReg){
     source2 = source2 - 32;
        reservation_stations.entries[thisStationNumber].value2=float2unsigned(get_fp_register(source2));
    }
    else{
        reservation_stations.entries[thisStationNumber].value2=get_int_register(source2);
    }

  }

}

void sim_ooo::single_argument_tag_check(map_entry_t thisMapEntry){
  unsigned setTag = UNDEFINED;
  unsigned setVal = UNDEFINED;
  unsigned thisStationNumber = thisMapEntry.resStationIndex;
  unsigned thisRobIndex = thisMapEntry.robIndex;
  unsigned source1 = instr_memory[thisMapEntry.instrMemoryIndex].src1;
  bool waitOnValue = false;
  unsigned pcForMap = reservation_stations.entries[thisStationNumber].pc;
  if(is_int_imm(thisMapEntry.instructionOpcode)){
    reservation_stations.entries[thisStationNumber].value2 = UNDEFINED;
  }
  else{
    reservation_stations.entries[thisStationNumber].value2 =
      instr_memory[thisMapEntry.instrMemoryIndex].immediate;
  }

  setVal = find_value_in_rob(source1);
  if(setVal == UNDEFINED){
    setTag = find_tag_in_rob(source1,thisRobIndex);
    if(setTag == thisMapEntry.robIndex){
#ifdef BRANCH_EOP_DBG
      cout << "Self tag issue again" << endl;
      #endif
      setTag=UNDEFINED;
    }
  }
  if(setVal!=UNDEFINED){
    reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].value1 = setVal;
  }
  else if(setTag != UNDEFINED){
    reservation_stations.entries[thisStationNumber].tag1 = setTag;
    reservation_stations.entries[thisStationNumber].value1 = UNDEFINED;
    waitOnValue = true;
  }
  else {
    reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].value1 =
      int_gp[source1].value;
  }
#ifdef BRANCH_EOP_DBG
  if(thisMapEntry.instructionOpcode==SUBI){
    cout << "Searching for this Value in ROB: " << source1 << endl;
    cout << "Value deterimined to be: " << setVal << endl;
    cout << "Tag determined to be: " << setTag << endl;
  }
  print_reservation_stations();
  #endif
}

void sim_ooo::conditional_argument_tag_check(map_entry_t thisMapEntry){
  unsigned setTag = UNDEFINED;
  unsigned setVal = UNDEFINED;

  unsigned thisRobIndex = thisMapEntry.robIndex;
  unsigned thisStationNumber = thisMapEntry.resStationIndex;
  unsigned source1 = instr_memory[thisMapEntry.instrMemoryIndex].src1;
  bool waitOnValue = false;
  unsigned pcForMap = reservation_stations.entries[thisStationNumber].pc;

  reservation_stations.entries[thisStationNumber].value2 = UNDEFINED;
#ifdef CONFLICT_DBG
  #endif
  setTag = find_tag_in_rob(source1,thisRobIndex);
  if(setTag!=UNDEFINED){
    reservation_stations.entries[thisStationNumber].tag1 = setTag;
    reservation_stations.entries[thisStationNumber].value1 = UNDEFINED;
    return;
  }
  if(setTag == UNDEFINED){
    setVal = find_value_in_rob(source1);
    if(setVal!=UNDEFINED){
    reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].value1 = setVal;
    return;
    }
  }
  if((setTag==UNDEFINED)&&(setVal==UNDEFINED)){
    reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].value1 = int_gp[source1].value;
    return;
  }
  // setVal = find_value_in_rob(source1);
  // if(setVal == UNDEFINED){
  //   setTag = find_tag_in_rob(source1, thisRobIndex);
  // }
  // if(setVal!=UNDEFINED){
  //   reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
  //   reservation_stations.entries[thisStationNumber].value1 = setVal;
  // }
  // else if(setTag != UNDEFINED){
  //   reservation_stations.entries[thisStationNumber].tag1 = setTag;
  //   reservation_stations.entries[thisStationNumber].value1 = UNDEFINED;
  //   waitOnValue = true;
  // }
  // else {
  //   reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
  //   reservation_stations.entries[thisStationNumber].value1 =
  //       get_int_register(source1);
  // }
}

void sim_ooo::load_argument_tag_check(map_entry_t thisMapEntry){
  unsigned thisStationNumber = thisMapEntry.resStationIndex;
  unsigned thisRobIndex = thisMapEntry.robIndex;
  unsigned setTag = UNDEFINED;
  unsigned setVal = UNDEFINED;
  unsigned source = instr_memory[thisMapEntry.instrMemoryIndex].src1;
  unsigned destination = instr_memory[thisMapEntry.instrMemoryIndex].dest;
  bool intLoadInstruction = (thisMapEntry.instructionOpcode == LW);
  bool waitOnValue = false;
  unsigned pcForMap =   reservation_stations.entries[thisStationNumber].pc;
  reservation_stations.entries[thisStationNumber].tag2 = UNDEFINED;
  reservation_stations.entries[thisStationNumber].value2 = UNDEFINED;
  reservation_stations.entries[thisStationNumber].address = instr_memory[thisMapEntry.instrMemoryIndex].immediate;
  // get tag or value of
       #ifdef PRINT_DBG
  cout << "Source: R" << source << endl;
        #endif
  if (!intLoadInstruction) {
       #ifdef PRINT_DBG
    cout << "Destination: F" << destination << endl;
#endif
    destination = destination + 32;
  }
  setVal = find_value_in_rob(source);
  if(setVal == UNDEFINED){
    setTag = find_tag_in_rob(source,thisRobIndex);
  }
  // value  found in register
  if (setVal != UNDEFINED) {
       #ifdef PRINT_DBG
    cout << "Value found in rob: " << setVal << endl;
#endif
    reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].value1 = setVal;
  }
  //tag found in register
  else if(setTag!=UNDEFINED){
       #ifdef PRINT_DBG
    cout << "Tag found in rob: " << setTag << endl;
#endif
    reservation_stations.entries[thisStationNumber].value1 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].tag1 = setTag;
    waitOnValue = true;
  }
  else{
    unsigned registerValue = int_gp[source].value;
      reservation_stations.entries[thisStationNumber].value1 = registerValue;
       #ifdef PRINT_DBG
    cout << "Neither tag nor value found in rob, value found in memory: " << registerValue << endl;
#endif
  }
}

void sim_ooo::store_argument_tag_check(map_entry_t thisMapEntry){
      /*
       *value1 = destination register or undefined
       *tag1 = destination register tag or undefined
       *value2 = memory address held in argument register
       *SW F0 10(r2)
       *source = src1 = F0
       *either vj or qj
       *destination = src2 = r2
       *value2 = contents of r2
       */
  unsigned thisStationNumber = thisMapEntry.resStationIndex;
  unsigned currentRobIndex = thisMapEntry.robIndex;
  unsigned setTag1;
  unsigned setTag2;
  unsigned setVal1;
  unsigned setVal2;
  //value to be stored
  unsigned source1 = instr_memory[thisMapEntry.instrMemoryIndex].src1; ;

  //where the value will be stored
  unsigned source2 = instr_memory[thisMapEntry.instrMemoryIndex].src2;
  //check if sw or sws
  bool intStoreInstruction= (thisMapEntry.instructionOpcode == SW);
  bool waitOnValue = false;
  //sw res station entry = instruction's immediate field
  reservation_stations.entries[thisStationNumber].address = instr_memory[thisMapEntry.instrMemoryIndex].immediate;
  //value to be stored
  //if sws, correct for fp register in rob
  if(!intStoreInstruction){
    source1 +=32;
#ifdef STORE_DBG
    cout << "Source 1: F" << source1-32 << endl;
    #endif
  }
#ifdef STORE_DBG
  else{
    cout << "Source 1: R" << source1 << endl;
  }
  cout << "Source 2: R" << source2 <<endl;
#endif
  //handling value to be stored
  setTag1 = find_tag_in_rob(source1,currentRobIndex);
#ifdef STORE_DBG
  cout << setTag1 << endl;
#endif
  if(setTag1==UNDEFINED){
    setVal1 = find_value_in_rob(source1);
#ifdef STORE_DBG
  cout << setVal1 << endl;
#endif

  }
  //if in rob, set to tag value
  if (setTag1 != UNDEFINED) {
    reservation_stations.entries[thisStationNumber].tag1 = setTag1;
    reservation_stations.entries[thisStationNumber].value1 = UNDEFINED;
    waitOnValue = true;
  }
  //if not in rob, get content of that register
  else {
    //if sws convert float register to entry to unsigned value, add to res
    if(!intStoreInstruction){
      reservation_stations.entries[thisStationNumber].value1 =
          float2unsigned(get_fp_register(source1-32));
    }
    //if sw, store unsigned int register value to res
    else{
      reservation_stations.entries[thisStationNumber].value1 = get_int_register(source1);
    }
    //either way, set tag to undefined
    //reservation_stations.entries[thisStationNumber].tag1 = UNDEFINED;
  }
  //handling value where register will be stored
  setTag2 = find_tag_in_rob(source2,currentRobIndex);
#ifdef STORE_DBG
  cout << "Tag 2 Value: " << setTag2 << endl;
#endif
  if(setTag2==UNDEFINED){
    setVal2=find_value_in_rob(source2);
#ifdef STORE_DBG
  cout << "Value 2 Value: " << setVal2 << endl;
#endif
    if(setVal2!=UNDEFINED){
      reservation_stations.entries[thisStationNumber].tag2 = UNDEFINED;
      reservation_stations.entries[thisStationNumber].value2 = setVal2;
    }
  }
  if (setTag2 != UNDEFINED) {
    reservation_stations.entries[thisStationNumber].tag2 = setTag1;
    reservation_stations.entries[thisStationNumber].value2 = UNDEFINED;
    waitOnValue = true;
  }
  if((setVal2==UNDEFINED)&&(setTag2 == UNDEFINED)) {
    reservation_stations.entries[thisStationNumber].tag2 = UNDEFINED;
    reservation_stations.entries[thisStationNumber].value2 =
        get_int_register(source2);
  }
}

unsigned sim_ooo::find_value_in_rob(unsigned int thisReg){
#ifdef CONFLICT_DBG
        cout << "Value destination found in rob" << endl;
        #endif
  for(unsigned i = 0; i < rob.num_entries; i++){
    if(rob.entries[i].destination==thisReg){
#ifdef CONFLICT_DBG
        cout << "Value destination found in rob" << endl;
        #endif
      if(rob.entries[i].value!=UNDEFINED){
#ifdef CONFLICT_DBG
        cout << "Value found in rob" << endl;
        #endif
        return rob.entries[i].value;
      }
    }
  }
  return UNDEFINED;

}

unsigned sim_ooo::find_tag_in_rob(unsigned int thisRegister,
                                  unsigned currentRobIndex) {
  int maxIndex = rob.num_entries;
  unsigned newROBpc = rob.entries[currentRobIndex].pc;
  unsigned earliestFound = newROBpc;
  unsigned returnIndex = UNDEFINED;
  bool someValueFound = false;
  for (int i = maxIndex; i-- > 0;) {
    if (rob.entries[i].destination == thisRegister && (i != currentRobIndex)) {
      if (rob.entries[i].value == UNDEFINED) {
#ifdef CONFLICT_DBG
        cout << "Returning: " << i << endl;
#endif
        return i;
      }
    }
      }

  return UNDEFINED;
}

unsigned sim_ooo::get_tag(unsigned thisReg){
  for(unsigned i = 0; i < rob.num_entries; i++){
    if(rob.entries[i].destination==thisReg){
      return i;
    }
  }
  return UNDEFINED;
}

unsigned sim_ooo::get_res_station_index(unsigned thisPC){
  for(unsigned i = 0; i< reservation_stations.num_entries; i++){
    if(reservation_stations.entries[i].pc == thisPC){
      return i;
    }
  }
  return UNDEFINED;
}

void sim_ooo::reservation_station_stats(){
  unsigned memUnits = 0;
  unsigned fpAddUnits = 0;
  unsigned multUnits = 0;
  unsigned intUnits = 0;
  res_station_t stationType;
  for(unsigned i = 0; i< reservation_stations.num_entries; i++){
    stationType = reservation_stations.entries[i].type;
    switch(stationType){
      case INTEGER_RS:
        intUnits++;
        break;
      case ADD_RS:
        fpAddUnits++;
        break;
      case MULT_RS:
        multUnits++;
        break;
      case LOAD_B:
        memUnits++;
        break;
    }
  }

}

void sim_ooo::reservation_station_check(){
 
}

//////////////////////////////////
// INSTRUCTION WINDOW FUNCTIONS //
//////////////////////////////////
void sim_ooo::instruction_window_add(unsigned thisWindowIndex, unsigned thisPC){
  unsigned instruction_window_index = UNDEFINED;
  pending_instructions.entries[thisWindowIndex].pc = thisPC;
  pending_instructions.entries[thisWindowIndex].issue = clock_cycle;
}

void sim_ooo::instruction_window_remove(unsigned thisInstructionWindowIndex){
  pending_instructions.entries[thisInstructionWindowIndex].pc=UNDEFINED;
  pending_instructions.entries[thisInstructionWindowIndex].issue=UNDEFINED;
  pending_instructions.entries[thisInstructionWindowIndex].exe=UNDEFINED;
  pending_instructions.entries[thisInstructionWindowIndex].wr=UNDEFINED;
  pending_instructions.entries[thisInstructionWindowIndex].commit=UNDEFINED;
}
//////////////////////
// HELPER FUNCTIONS //
//////////////////////
bool sim_ooo::is_fp_instruction(opcode_t thisOpcode){
  if(thisOpcode>EOP){
    return true;
  }
  return false;
}

void sim_ooo::print_map_entry(unsigned thisKeyValue) {
  std::map<unsigned, map_entry_t>::iterator it;
    it = instruction_map.find(thisKeyValue);
    cout << "The key is: " << it->first << endl;
    cout << "The execution unit is: ";
    if (it->second.executionUnitNumber == UNDEFINED)
      cout << " UNDEFINED" << endl;
    else {
      cout << it->second.executionUnitNumber << endl;
    }
    cout << "The instruction window index is: "
         << it->second.instrWindowIndex << endl;
    cout << "The rob index is: " << it->second.robIndex << endl;
    cout << "The res station index is: " << it->second.resStationIndex
         << endl;
    cout << "Is the instruction ready to write?" << endl;
    if (it->second.readyToWrite)
      cout << "Yes" << endl;
    else
      cout << "No" << endl;
    cout << "Is the instruction ready to commit?" << endl;
    if(it->second.readyToCommit)
      cout << "Yes";
    else
      cout << "No" << endl;
    cout << "\n" << endl;
}

void sim_ooo::print_map_entry(map_entry_t thisMapEntry){
  unsigned thisPC = reservation_stations.entries[thisMapEntry.resStationIndex].pc;
  cout << "PC of instruction: "  << thisPC << endl;
    cout << "Instruction window index: "
         << thisMapEntry.instrWindowIndex << endl;
    cout << "ROB index: " << thisMapEntry.robIndex << endl;
    cout << "Res. station index: " << thisMapEntry.resStationIndex
         << endl;
    cout << "Execution unit: ";
    if (thisMapEntry.executionUnitNumber == UNDEFINED)
      cout << " UNDEFINED" << endl;
    else {
      cout << thisMapEntry.executionUnitNumber << endl;
    }
    cout << "Ready to write: ";
    if (thisMapEntry.readyToWrite)
      cout << "Yes" << endl;
    else
      cout << "No" << endl;
    cout << "Ready to commit: ";
    if(thisMapEntry.readyToCommit)
      cout << "Yes";
    else
      cout << "No" << endl;
    cout<<"Values will be avialble at clock cycle: ";
    if(thisMapEntry.valuesAvailable==UNDEFINED){
      cout << "UNDEFINED" << endl;
    }
    else
      cout << thisMapEntry.valuesAvailable<< endl;
    cout << "\n" << endl;
}

opcode_t sim_ooo::pc_to_opcode_type(unsigned thisPC){
  unsigned instruction_index = thisPC/4;
  return instr_memory[instruction_index].opcode;
}
//take opcode return type
void print_instruction_type(opcode_t thisOpcode){
  if(is_fp_alu(thisOpcode)){
    cout << "FP Alu instruction\n" << endl;
  }
  if(is_memory(thisOpcode)){
    cout << "Memory instruction\n" << endl;
  }
  if(is_int_imm(thisOpcode)){
    cout << "Int immediate instruction\n" << endl;
  }
  if(thisOpcode==XOR){
    cout << "XOR instruction" << endl;
  }
}

void sim_ooo::print_pc_to_instruction(unsigned thisPC){
  unsigned instructionIndex = thisPC/4;
  instruction_t thisInstruction = instr_memory[instructionIndex];
  cout << "This instruction is -" << endl;
  print_string_opcode(thisInstruction.opcode);
  cout << "Source 1: " << thisInstruction.src1 << endl;
  cout << "Source 2: " << thisInstruction.src2 << endl;
  cout << "Destination: " << thisInstruction.dest << endl;
  cout << "\n" << endl;

}
/////////////////////////////////////////////
// CHECKING RESERVATION STATION ARGUMENTS  //
/////////////////////////////////////////////
bool sim_ooo::arguments_ready_find(unsigned int thisPC) {
  std::map<unsigned, map_entry_t>::iterator tempMapEntry;
  opcode_t instructionOpcode;
  unsigned res_station_index;
  unsigned tempPC;
  // locate instructions not executed in instruction window
  // get instruction type

  tempMapEntry = instruction_map.find(thisPC);
  //get opcode
  instructionOpcode = pc_to_opcode_type(thisPC);
  #ifdef EXECUTE_DBG
  cout << "Instruction type: " << endl;
  print_instruction_type(instructionOpcode);
  #endif
  // get reservation station location
  res_station_index = tempMapEntry->second.resStationIndex;
  // error check, cant find instruction in reservation station
  if (res_station_index == UNDEFINED) {
#ifdef EXECUTE_DBG
    cout << "Can't locate instruction in reservation station structure" << endl;
    #endif
    return false;
  }
  // check double argument instruction
  if (is_int_r(instructionOpcode)||is_fp_alu(instructionOpcode)) {
#ifdef EXECUTE_DBG
    if(instructionOpcode == XOR)
    cout << "Checking XOR instruction" << endl;
      bool argsReady = arguments_ready_fp_alu(res_station_index);
    cout << "Args ready: ";
      cout << argsReady << endl;;
    #endif
    return arguments_ready_fp_alu(res_station_index);
  }
  // check single argument instruction
  if (is_int_imm(instructionOpcode)) {
    return arguments_ready_int_imm(res_station_index);
  }
  //check conditional
  if(is_conditional(instructionOpcode)){
    return arguments_ready_conditional(res_station_index);
  }
  // check memory
  if (is_memory(instructionOpcode)) {
    // check load arguments
    if (instructionOpcode == LW || instructionOpcode == LWS) {
      return arguments_ready_load(res_station_index);
    }
    // check store arguments
    else {
      return arguments_ready_store(res_station_index);
    }
  }

  return false;
}

//check if load reservation stations are ready
bool sim_ooo::arguments_ready_load(unsigned res_station_index){
  std::map<unsigned, map_entry_t>::iterator it;
  it = instruction_map.find(reservation_stations.entries[res_station_index].pc);
  map_entry_t tempMapEntry = it->second;
  unsigned mapEntryPC = it->first;
  unsigned val1 = reservation_stations.entries[res_station_index].value1;
  unsigned nextMemoryInstruction = memory_instruction_q.front();
   #ifdef STORE_DBG
  cout << "Next memory instruction in queue: " << nextMemoryInstruction << endl;
  cout << "This instructions PC: " << mapEntryPC <<endl;
    #endif
  if(mapEntryPC!=nextMemoryInstruction){
   #ifdef STORE_DBG
    cout << "Out of order memory instruction" << endl;
    #endif
    return false;
  }
#ifdef PRINT_DBG
  cout<< "Checking LOAD instruction's arguments." << endl;
  print_map_entry(tempMapEntry);
#endif
  if (tempMapEntry.valuesAvailable == UNDEFINED) {
    #ifdef PRINT_DBG
    cout << "Values not available" << endl;
    #endif
    return false;
  }
  if(tempMapEntry.valuesAvailable!=UNDEFINED){

#ifdef PRINT_DBG
cout<< "current clock cycle: " << clock_cycle << endl;
  #endif
    if(clock_cycle<tempMapEntry.valuesAvailable){

#ifdef PRINT_DBG

      cout << "Values not available until clock cycle ";
      cout << tempMapEntry.valuesAvailable << endl;
  #endif
      return false;
    }
    else{
      if (val1 != UNDEFINED) {

#ifdef PRINT_DBG

  cout << "LOAD instruction's value1: " << val1 << endl;
        cout << "This instruction is ready.\n" << endl;
  #endif
        //memory_instruction_q.pop();
        return true;
      }

#ifdef PRINT_DBG

    cout << "Instruction is not ready to execute" << endl;
      cout << "Instruction's argmuents not available." << endl;
      print_culprit(val1);
  #endif
      return false;
    }
}
  return false;
}

//check if store reservation stations are ready
bool sim_ooo::arguments_ready_store(unsigned res_station_index) {
  std::map<unsigned, map_entry_t>::iterator it;
  it = instruction_map.find(reservation_stations.entries[res_station_index].pc);
  map_entry_t tempMapEntry = it->second;
  unsigned mapEntryPC = it->first;
  unsigned val1 = reservation_stations.entries[res_station_index].value1;
  unsigned val2 = reservation_stations.entries[res_station_index].value2;
  unsigned nextMemoryInstruction = memory_instruction_q.front();
   #ifdef STORE_DBG
  cout << "(store)Next memory instruction in queue: " << nextMemoryInstruction << endl;
  cout << "This instructions PC: " << mapEntryPC <<endl;
    #endif
  if(mapEntryPC!=nextMemoryInstruction){
   #ifdef STORE_DBG
    cout << "Out of order memory instruction" << endl;
    #endif
    return false;
  }

#ifdef PRINT_DBG
  cout << "Checking STORE instruction's arguments." << endl;
  cout << "Instruction's reservation station index: " << res_station_index
       << endl;
  #endif

  if (tempMapEntry.valuesAvailable == UNDEFINED) {
    #ifdef PRINT_DBG
    cout << "Values not availabel" << endl;
    #endif
    return false;
  }
  if (tempMapEntry.valuesAvailable != UNDEFINED) {
    if (clock_cycle < tempMapEntry.valuesAvailable) {
    #ifdef PRINT_DBG
      cout << "Values not available until clock cycle ";
      cout << tempMapEntry.valuesAvailable << endl;
    #endif
      return false;
    } else {
      if ((val1 != UNDEFINED) && (val2 != UNDEFINED)) {
    #ifdef PRINT_DBG
        cout << "This instruction is ready.\n" << endl;
    #endif

        return true;
      }
    #ifdef PRINT_DBG
      cout << "Instruction is not ready to execute" << endl;
      cout << "Instruction's argmuents not available." << endl;
      print_culprit(val1, val2);
    #endif
      return false;
    }
  }
  return false;
}

//check if single argument reservation stations are ready
bool sim_ooo::arguments_ready_int_imm(unsigned res_station_index){
  map_entry_t tempMapEntry = instruction_map.find(reservation_stations.entries[res_station_index].pc)->second;
  unsigned val1 = reservation_stations.entries[tempMapEntry.resStationIndex].value1;
  if (tempMapEntry.valuesAvailable == UNDEFINED) {
    #ifdef PRINT_DBG
    cout << "Values not available" << endl;
    #endif
    return false;
  }
  if(tempMapEntry.valuesAvailable!=UNDEFINED){
    if(clock_cycle<tempMapEntry.valuesAvailable){
    #ifdef PRINT_DBG
      cout << "Values not available until clock cycle ";
      cout << tempMapEntry.valuesAvailable << endl;
    #endif
      return false;
    }
    else{
      if (val1 != UNDEFINED) {
        #ifdef PRINT_DBG
        cout << "Imm instruction's value1: " << val1 << endl;
        cout << "This instruction is ready.\n" << endl;
        #endif
        return true;
      }

        #ifdef PRINT_DBG
      cout << "Instruction is not ready to execute" << endl;
      cout << "Instruction's argmuents not available." << endl;
      print_culprit(val1);
      #endif
      return false;
    }
}
  return false;
}

//check if double argument reservation stations are ready
bool sim_ooo::arguments_ready_fp_alu(unsigned res_station_index){
        #ifdef PRINT_DBG
  cout<< "Checking FP instruction's arguments." << endl;
  cout<< "Instruction's reservation station index: " << res_station_index<< endl;
  #endif
  map_entry_t tempMapEntry = instruction_map.find(reservation_stations.entries[res_station_index].pc)->second;
  unsigned val1 = reservation_stations.entries[res_station_index].value1;
  unsigned val2 = reservation_stations.entries[res_station_index].value2;
  unsigned tag1 = reservation_stations.entries[res_station_index].tag1;
  unsigned tag2 = reservation_stations.entries[res_station_index].tag2;
  if(tempMapEntry.instructionOpcode==XOR){
#ifdef XOR_DBG
      cout << "Clearing xor for execution" << endl;
#endif
      if (tag1==UNDEFINED && tag2==UNDEFINED) {
#ifdef XOR_DBG
        cout << "xor cleared!" << endl;
#endif
        return true;
      }
  }
  if (tempMapEntry.valuesAvailable == UNDEFINED) {
        #ifdef PRINT_DBG
    cout << "Values not available" << endl;
    #endif
    return false;
  }
  if(tempMapEntry.valuesAvailable!=UNDEFINED){
    if(clock_cycle<tempMapEntry.valuesAvailable){
        #ifdef PRINT_DBG
      cout << "Values not available until clock cycle ";
      cout << tempMapEntry.valuesAvailable << endl;
      #endif
      return false;
    }
    else{
      if ((val1 != UNDEFINED) || (val2 != UNDEFINED)) {
        if (val1 != UNDEFINED && val2 != UNDEFINED) {
        #ifdef PRINT_DBG
          cout << "This instruction is ready." << endl;
      #endif
          return true;
        }
      }
        #ifdef PRINT_DBG
      cout << "Instruction is not ready to execute" << endl;
      cout << "Instruction's argmuents not available." << endl;
      print_culprit(val1, val2);
      #endif
      return false;
    }
  }
  return false;
}

//check if conditional arguments ready
bool sim_ooo::arguments_ready_conditional(unsigned int res_station_index){
#ifdef BRANCH_EOP_DBG
  cout<< "Checking conditional instruction's arguments." << endl;
  cout<< "Instruction's reservation station index: " << res_station_index<< endl;
  #endif
  map_entry_t tempMapEntry = instruction_map.find(reservation_stations.entries[res_station_index].pc)->second;
  unsigned val1 = reservation_stations.entries[res_station_index].value1;
#ifdef BRANCH_EOP_DBG
    cout << "Value1: " << val1 << endl;
    print_reservation_stations();
  #endif
  if (tempMapEntry.valuesAvailable == UNDEFINED) {
#ifdef BRANCH_EOP_DBG
    cout << "Values not available" << endl;
  #endif
    return false;
  }
  if(tempMapEntry.valuesAvailable!=UNDEFINED){
    if(clock_cycle<tempMapEntry.valuesAvailable){
#ifdef BRANCH_EOP_DBG
      cout << "Values not available until clock cycle ";
      cout << tempMapEntry.valuesAvailable << endl;
  #endif
      return false;
    }
    else{
        if (val1 != UNDEFINED) {
#ifdef BRANCH_EOP_DBG
          cout << "This instruction is ready." << endl;
  #endif
          return true;
        }

#ifdef BRANCH_EOP_DBG
      cout << "Instruction is not ready to execute" << endl;
      cout << "Instruction's argmuents not available." << endl;
      cout << "Value1 is UNDEFINED" << endl;
  #endif
      return false;
    }
  }
  return false;
}

void print_culprit(unsigned thisVal1, unsigned thisVal2){
  if(thisVal1 == UNDEFINED)
    cout << "Culprit: value1 -> UNDEFINED" << endl;
  if(thisVal2 == UNDEFINED)
    cout << "Culprit: value2 -> UNDEFINED" << endl;
  cout<<"\n";
}

void print_string_opcode(opcode_t thisOpcode) {
  switch (thisOpcode) {
  case LW:
    cout << "Opcode is LW" << endl;
    break;
  case SW:
    cout << "Opcode is SW" << endl;
    break;
  case ADD:
    cout << "Opcode is ADD" << endl;
    break;
  case ADDI:
    cout << "Opcode is ADDI" << endl;
    break;
  case SUB:
    cout << "Opcode is SUB" << endl;
    break;
  case SUBI:
    cout << "Opcode is SUBI" << endl;
    break;
  case XOR:
    cout << "Opcode is XOR" << endl;
    break;
  case AND:
    cout << "Opcode is AND" << endl;
    break;
  case MULT:
    cout << "Opcode is MULT" << endl;
    break;
  case DIV:
    cout << "Opcode is DIV" << endl;
    break;
  case BEQZ:
    cout << "Opcode is BEQZ" << endl;
    break;
  case BNEZ:
    cout << "Opcode is BNEZ" << endl;
    break;
  case BLTZ:
    cout << "Opcode is BLTZ" << endl;
    break;
  case BGTZ:
    cout << "Opcode is BGTZ" << endl;
    break;
  case BLEZ:
    cout << "Opcode is BLEZ" << endl;
    break;
  case BGEZ:
    cout << "Opcode is BGEZ" << endl;
    break;
  case JUMP:
    cout << "Opcode is JUMP" << endl;
    break;
  case EOP:
    cout << "Opcode is EOP" << endl;
    break;
  case LWS:
    cout << "Opcode is LWS" << endl;
    break;
  case SWS:
    cout << "Opcode is LWS" << endl;
    break;
  case ADDS:
    cout << "Opcode is ADDS" << endl;
    break;
  case SUBS:
    cout << "Opcode is SUBS" << endl;
    break;
  case MULTS:
    cout << "Opcode is MULTS" << endl;
    break;
  case DIVS:
    cout << "Opcode is DIVS" << endl;
    break;
  }
}

/////////////////////////
// EXECUTION FUNCTIONS //
/////////////////////////
void sim_ooo::execute(){
  //locate instructions that haven't begun execution
  find_pending_execution_instructions();
  // begin execution of available instructions in issue stage
  cycle_execution_units();
}

//locate instructions that haven't begun execution
void sim_ooo::find_pending_execution_instructions() {
#ifdef EXECUTE_DBG
  cout << "****FINDING INSTRUCTIONS THAT ARE READY TO BEGIN EXECUTION****"
       << endl;
#endif
  unsigned pcOfInstruction;
  unsigned numPending = 0;
  bool executionProceed = false;
  std::map<unsigned, map_entry_t>::iterator it;
  for (unsigned i = 0; i < pending_instructions.num_entries; i++) {
    if ((pending_instructions.entries[i].exe == UNDEFINED) &&
        (pending_instructions.entries[i].issue != UNDEFINED)) {
      numPending++;
      pcOfInstruction = pending_instructions.entries[i].pc;
#ifdef EXECUTE_DBG
      cout << "The following instruction has not begun execution: " << endl;
      print_pc_to_instruction(pcOfInstruction);
      cout << "Attempting to begin execution." << endl;
#endif
    #ifdef DIV_DBG
      #endif
      // check if arguments are ready
      if (arguments_ready_find(pcOfInstruction)) {
        // if arguments are ready, find execution unit
        find_available_execution_unit(pcOfInstruction);
      } else {
#ifdef EXECUTE_DBG
        cout << "Execution did not begin." << endl;
        cout << "Arguments not ready." << endl;
#endif
      }
    }
  }
  if (!numPending) {
#ifdef EXECUTE_DBG
    cout << "No instructions are pending execution." << endl;
#endif
  }
}

//find an execution unit that will house this instruction
void sim_ooo::find_available_execution_unit(unsigned thisPC){
  opcode_t thisOpcode = pc_to_opcode_type(thisPC);
#if PRINT_EXECUTION_UNITS
  cout << "**printing execution units**" << endl;
  print_active_execution_units();
  cout << "*searching for execution unit*" << endl;
#endif
  unsigned execUnitNumber = get_free_unit(thisOpcode);
  if (execUnitNumber != UNDEFINED) {
    // claim execution unit
    if(is_memory(thisOpcode)){
#ifdef STORE_DBG
    cout << "claiming execution unit" << endl;
    cout << "Next memory instruction in q: " << memory_instruction_q.front() << endl;
    cout << "This instruction: " << thisPC << endl;
  #endif
    memory_instruction_q.pop();
    }

#ifdef PRINT_DBG
    cout << "claiming execution unit" << endl;
  #endif
    claim_execution_unit(execUnitNumber, thisPC);
#ifdef PRINT_DBG
    cout << "execution has begun" << endl;
  #endif
#if PRINT_EXECUTION_UNITS
    cout << "Current active execution units: " << endl;
    print_active_execution_units();
#endif
    // add to map
    map_entry_t *thisMapEntry = &instruction_map.find(thisPC)->second;
    // update map entry
    // get instruction window index from map entry
    unsigned instructionWindowIndex = thisMapEntry->instrWindowIndex;
    thisMapEntry->executionUnitNumber = execUnitNumber;
    // add execution clock cycle info
    pending_instructions.entries[thisMapEntry->instrWindowIndex].exe =
        clock_cycle;
    // update rob
    // change rob state to execute
    rob.entries[thisMapEntry->robIndex].state = EXECUTE;
    if(is_memory(thisOpcode)){
      if(thisOpcode==LWS||thisOpcode==LW){
        reservation_stations.entries[thisMapEntry->resStationIndex].address =
            reservation_stations.entries[thisMapEntry->resStationIndex].value1 +
            reservation_stations.entries[thisMapEntry->resStationIndex].address;
      }
      if(thisOpcode==SWS||thisOpcode==SW){
        unsigned updatedAddress = reservation_stations.entries[thisMapEntry->resStationIndex].value2 + reservation_stations.entries[thisMapEntry->resStationIndex].address;
        unsigned value = reservation_stations.entries[thisMapEntry->resStationIndex].value1;
        reservation_stations.entries[thisMapEntry->resStationIndex].address = updatedAddress;
        rob.entries[thisMapEntry->robIndex].destination=updatedAddress;
        rob.entries[thisMapEntry->robIndex].value = value;
      }
    }
  } else {
    #ifdef EXECUTE_DBG
    cout << "Execution unit not available." << endl;
    #endif
  }
}

//claim this execution unit
void sim_ooo::claim_execution_unit(unsigned int thisUnit, unsigned thisPC){
  //set this execution unit's pc field to this pc
 #ifdef PRINT_DBG
  cout << "Execution unit " << thisUnit << " claimed for this instruction" << endl;
  #endif

  exec_units[thisUnit].pc = thisPC;
  //set this execution unit's busy field to the latency of these types of units

  exec_units[thisUnit].busy = exec_units[thisUnit].latency;
#ifdef PRINT_DBG
  cout << "Beginning execution of this instruction at clock cycle: "
       << clock_cycle << endl;
  cout << "Instruction should write result at clock cycle: "
       << clock_cycle + exec_units[thisUnit].latency << endl;
  #endif
}

//decrement all active execution units
void sim_ooo::cycle_execution_units(){
#ifdef PRINT_DBG
  cout<< "****CYCLING EXECUTION UNITS****" << endl;
  #endif
  unsigned execUnitNumber = UNDEFINED;
  opcode_t thisOpcode;
  std::map<unsigned, map_entry_t>::iterator it;
  it=instruction_map.begin();
  while(it!=instruction_map.end()){
    thisOpcode = it->second.instructionOpcode;
    execUnitNumber = it->second.executionUnitNumber;
    if(execUnitNumber!=UNDEFINED){
      if ((thisOpcode != SW) && (thisOpcode != SWS)) {
        if (exec_units[execUnitNumber].busy >= 0) {
#ifdef PRINT_DBG
          cout << "Cycling unit: " << execUnitNumber << endl;
          cout << "Execution cycles remaining: ";
          cout << exec_units[execUnitNumber].busy << endl;
          print_string_unit_type(exec_units[execUnitNumber].type);
#endif
          if (exec_units[execUnitNumber].busy == 1) {
            it->second.readyToWrite = true;
          }
          if (exec_units[execUnitNumber].busy == 0) {
            it->second.executionUnitNumber = UNDEFINED;
            exec_units[execUnitNumber].pc = UNDEFINED;
          }
          if (exec_units[execUnitNumber].busy > 0) {
            exec_units[execUnitNumber].busy--;
          }
        }
#ifdef PRINT_DBG
        cout << "New execution cycles remaining: "
             << exec_units[execUnitNumber].busy << "\n"
             << endl;
#endif
      }
      else {
#ifdef STORE_DBG
        cout << "Store execution unit found" << endl;
#endif
        unsigned writeTime = exec_units[execUnitNumber].latency;
        if (exec_units[execUnitNumber].busy >= 0) {
#ifdef PRINT_DBG
          cout << "Cycling unit: " << execUnitNumber << endl;
          cout << "Execution cycles remaining: ";
          cout << exec_units[execUnitNumber].busy << endl;
          print_string_unit_type(exec_units[execUnitNumber].type);
#endif
          if (exec_units[execUnitNumber].busy == writeTime) {
#ifdef STORE_DBG
            cout << "Signaling store instruction to move ahead to write result" << endl;
            #endif
            it->second.readyToWrite = true;
          }
          if (exec_units[execUnitNumber].busy == 0) {
            it->second.executionUnitNumber = UNDEFINED;
            exec_units[execUnitNumber].pc = UNDEFINED;
          }
          if (exec_units[execUnitNumber].busy > 0) {
            exec_units[execUnitNumber].busy--;
          }
        }
      }
    }
    it++;
  }
#ifdef PRINT_DBG
  cout<< "****DONE CYCLING EXECUTION UNITS****\n" << endl;
  #endif
}

void sim_ooo::print_cycle_info(map_entry_t thisMapEntry){

  cout << "this instruction will write next clock cycle" << endl;
}

//preform the instruction action
void sim_ooo::take_instruction_action(map_entry_t thisMapEntry){
  unsigned thisPC = reservation_stations.entries[thisMapEntry.resStationIndex].pc;

#ifdef PRINT_DBG
  cout << "\n***Taking Instruction Action***"<<endl;
  print_pc_to_instruction(thisPC);
  #endif
  opcode_t thisOpcode = thisMapEntry.instructionOpcode;
  if((thisOpcode!=LW)&&(thisOpcode!=LWS)){
    //do fp adds subs mults or divs
    if(is_fp_alu(thisOpcode)){
      fp_instruction_action(thisMapEntry);
    }
    else if(is_int_imm(thisOpcode)){
#ifdef BRANCH_EOP_DBG
      if(thisOpcode==SUBI){
        cout << "SUBI is making here" << endl;
      }
      if(thisOpcode==ADDI){
        cout << "ADDI is making it here" << endl;
      }
#endif
      int_imm_instruction_action(thisMapEntry);
    }
    else if(is_int_r(thisOpcode)){
      int_instruction_action(thisMapEntry);
    }
    else if(is_branch(thisOpcode)){
      conditional_instruction_action(thisMapEntry);
    }
    return;
  } else {
    if(is_memory(thisOpcode)){
      if(thisOpcode==LWS){
        lws_instruction_action(thisMapEntry);
    }
      if(thisOpcode==SWS){
        return;
      }
  }

#ifdef PRINT_DBG
  cout << "***Done taking Instruction Action***\n"<<endl;
  #endif
  }
}
void sim_ooo::take_store_action(unsigned int thisValue, unsigned int thisDestination){
  write_memory(thisDestination, thisValue);
}
void sim_ooo::fp_instruction_action(map_entry_t thisMapEntry){
  unsigned value1 =
      reservation_stations.entries[thisMapEntry.resStationIndex].value1;
  unsigned value2 =
      reservation_stations.entries[thisMapEntry.resStationIndex].value2;
  unsigned thisPC =

    reservation_stations.entries[thisMapEntry.resStationIndex].pc;
#ifdef PRINT_DBG
  cout << "Taking alu action" << endl;
  #endif
  unsigned aluReturnValue = alu(thisMapEntry.instructionOpcode, value1, value2, UNDEFINED, thisPC);
  rob.entries[thisMapEntry.robIndex].value = aluReturnValue;
  rob.entries[thisMapEntry.robIndex].ready = true;
  thisMapEntry.readyToWrite = true;
  // floating point alu action (ADDS,SUBS,MULTS,DIVS)
}

void sim_ooo::int_instruction_action(map_entry_t thisMapEntry){
  unsigned value1 =
      reservation_stations.entries[thisMapEntry.resStationIndex].value1;
  unsigned value2 =
      reservation_stations.entries[thisMapEntry.resStationIndex].value2;
  unsigned thisPC =
      reservation_stations.entries[thisMapEntry.resStationIndex].pc;

#ifdef PRINT_DBG
  cout << "Taking alu action" << endl;
  #endif

  unsigned aluReturnValue = alu(thisMapEntry.instructionOpcode, value1, value2, UNDEFINED, thisPC);
  rob.entries[thisMapEntry.robIndex].value = aluReturnValue;
  rob.entries[thisMapEntry.robIndex].ready = true;
  thisMapEntry.readyToWrite = true;
 
}

void sim_ooo::int_imm_instruction_action(map_entry_t thisMapEntry){
  unsigned thisPC =reservation_stations.entries[thisMapEntry.resStationIndex].pc;
  unsigned instructionIndex = thisPC/4;
  unsigned value1 =
      reservation_stations.entries[thisMapEntry.resStationIndex].value1;
  unsigned value2= instr_memory[instructionIndex].immediate;
#ifdef PRINT_DBG
  cout << "Taking alu action" << endl;
  #endif

  unsigned aluReturnValue = alu(thisMapEntry.instructionOpcode, value1, value2, UNDEFINED, thisPC);
#ifdef IMM_DBG
  cout << "Value 1: " << value1 << endl;
  cout << "Value 2: " << value2 << endl;
  cout << "ALU return value: " << aluReturnValue << endl;
#endif
  rob.entries[thisMapEntry.robIndex].value = aluReturnValue;
  rob.entries[thisMapEntry.robIndex].ready = true;
  thisMapEntry.readyToWrite = true;
  
}

void sim_ooo::conditional_instruction_action(map_entry_t thisMapEntry){
  unsigned instructionIndex = rob.entries[thisMapEntry.robIndex].pc/4;
  unsigned immediate = instr_memory[instructionIndex].immediate;
  unsigned thisPC = reservation_stations.entries[thisMapEntry.resStationIndex].pc;
  unsigned pcOfTarget = immediate*4;
        #ifdef PRINT_DBG
  cout << "Target is instruction: " << immediate << endl;
  print_pc_to_instruction(pcOfTarget);
#endif
  unsigned value1 = reservation_stations.entries[thisMapEntry.resStationIndex].value1;
  unsigned aluReturnValue = alu(thisMapEntry.instructionOpcode, value1, UNDEFINED, immediate, thisPC);
  rob.entries[thisMapEntry.robIndex].value = aluReturnValue;
  rob.entries[thisMapEntry.robIndex].ready = true;
  thisMapEntry.readyToWrite = true;
}

//print the execution units that are busy right now
void sim_ooo::print_active_execution_units(){
  unit_t busyUnit;
  for(unsigned i = 0; i<num_units; i++){
    cout << "Execution unit " << i << ": ";
    if(exec_units[i].pc!=UNDEFINED){

#ifdef PRINT_DBG

      cout << " busy" << endl;
      cout << "This execution unit is processing instruction: " << busyUnit.pc << endl;
  #endif
    }
    else{

#ifdef PRINT_DBG
cout << " free" << endl;
      cout << "PC: " << exec_units[i].pc << endl;
  #endif
    }

#ifdef PRINT_DBG
    print_string_unit_type(exec_units[i].type);
    cout << "Busy: " << exec_units[i].busy << endl;
    cout << "Latency: " << exec_units[i].latency << "\n" << endl;
  #endif
  }
}

//print a string for the execution unit, instead of the enumeration value
void print_string_unit_type(exe_unit_t thisUnit){
  cout << "Type: ";
  if (thisUnit == INTEGER) {
    cout << "Integer";
  }
  if (thisUnit == ADDER) {
    cout << "Adder";
  }
  if (thisUnit == MULTIPLIER) {
    cout << "Multiplier";
  }
  if (thisUnit == DIVIDER) {
    cout << "Divider";
  }
  if (thisUnit == MEMORY) {
    cout << "Memory";
  }
  cout << endl;
}

unsigned sim_ooo::get_unsigned_memory_value(unsigned thisIndexOfMemory){
  return data_memory[thisIndexOfMemory];
}

void sim_ooo::eop_execution(){
  unsigned eopUnitNumber = get_free_unit(EOP);
  if(eopUnitNumber){
    exec_units[eopUnitNumber].busy=exec_units[eopUnitNumber].latency;
  }
}

void print_memory_update(unsigned thisValue, unsigned thisOffset,
                         unsigned thisMemIndex, unsigned thisConvertedValue){
  cout << "Value 1: " << thisValue << endl;
  cout << "Value 2: " << thisOffset << endl;
  cout << "Data memory index: " << thisMemIndex << endl;
  cout << "Accessing memory at index: " << thisMemIndex << endl;
  cout << "Memory value after converting to unsigned: " << thisConvertedValue
       << endl;
  cout<< "Formatting two: ";
  cout << hex << int(thisConvertedValue) << endl;
  cout<< "Formatting three: ";
  cout << hex << setw(2) << setfill('0') << int(thisConvertedValue) << "\n";
}

//////////////////////
// MEMORY FUNCTIONS //
//////////////////////
void sim_ooo::lws_instruction_action(map_entry_t thisMapEntry){
       #ifdef PRINT_DBG
        cout << "Performing LWS" << endl;
        cout << "Getting instruction" << endl;
        print_pc_to_instruction(reservation_stations.entries[thisMapEntry.resStationIndex].pc);
#endif
        //get values
        //register value and offset for lws
        unsigned address= reservation_stations.entries[thisMapEntry.resStationIndex].address;
        //compute address
        //buffer for data
        unsigned char* dataFromMemory;
        dataFromMemory = &data_memory[address];
        unsigned convertedValue = char2unsigned(dataFromMemory);
//        print_memory_update(registerValue, offset, dataMemoryIndex, convertedValue);
        //update rob
        rob.entries[thisMapEntry.robIndex].value = convertedValue;
        rob.entries[thisMapEntry.robIndex].ready = true;
        rob.entries[thisMapEntry.robIndex].state = WRITE_RESULT;
        //update execution unit
}

 /////////////////////////////
 // WRITE RESULT FUNCTIONS  //
 /////////////////////////////
void sim_ooo::write_results(){
  //find instructions that are ready to write results
  //for those instructions, replace reservation tag values with actual values
        #ifdef PRINT_DBG
  cout << "****FINDING INSTRUCTIONS THAT ARE READY TO WRITE****" << endl;
#endif
  find_ready_to_write();

}


void sim_ooo::find_ready_to_write(){
  std::map<unsigned, map_entry_t>::iterator it;
  bool noInstructionsReadyToWrite = true;
  opcode_t thisOpcode;
  it = instruction_map.begin();
  while(it!=instruction_map.end()){
    unsigned key = it->first;
    map_entry_t* tempMapEntry = &it->second;
    thisOpcode = tempMapEntry->instructionOpcode;
    if((thisOpcode!=SW)&&(thisOpcode!=SWS)){
      if (tempMapEntry->readyToWrite) {
#ifdef PRINT_DBG
        cout << "Check clock cycle: " << clock_cycle << endl;
        print_write_status(tempMapEntry->readyToWrite);
        cout << "****WRITING RESULTS****" << endl;
#endif
        noInstructionsReadyToWrite = false;
// update the reservation stations with matching tag value
#ifdef EARLY_COMMIT_DBG
        if (tempMapEntry->instructionOpcode == SUBS) {
          cout << "The SUBS writes now" << endl;
        }
#endif
        take_instruction_action(*tempMapEntry);
        res_stations_to_update.push(tempMapEntry->resStationIndex);
        res_stations_to_clear.push(tempMapEntry->resStationIndex);
//      update_reservation_stations(*tempMapEntry);
#ifdef PRINT_DBG
        cout << "****DONE WRITING RESULTS****" << endl;
        cout << "***RELEASING RESERVATION STATION OF THIS INSTRUCTION***"
             << endl;
#endif
        // clear tag values, clear value values, clear destination,
        tempMapEntry->readyToWrite = false;
#ifdef PRINT_DBG
        cout << "**reservation station release**" << endl;
        cout << "*approving commit*" << endl;
#endif
        tempMapEntry->readyToCommit = true;
#ifdef PRINT_DBG
        cout << "After approving commit, map entry: " << endl;
#endif
        pending_instructions.entries[tempMapEntry->instrWindowIndex].wr =
            clock_cycle;
        rob.entries[tempMapEntry->robIndex].state = WRITE_RESULT;
#ifdef PRINT_DBG
        print_map_entry(key);
#endif
      }
    }
    else{
      if(tempMapEntry->readyToWrite){
#ifdef STORE_DBG
      cout << "Writing store instruction results" << endl;
#endif
        res_stations_to_update.push(tempMapEntry->resStationIndex);
        res_stations_to_clear.push(tempMapEntry->resStationIndex);
//      update_reservation_stations(*tempMapEntry);
#ifdef PRINT_DBG
        cout << "****DONE WRITING RESULTS****" << endl;
        cout << "***RELEASING RESERVATION STATION OF THIS INSTRUCTION***"
             << endl;
#endif
        // clear tag values, clear value values, clear destination,
        tempMapEntry->readyToWrite = false;
#ifdef PRINT_DBG
        cout << "**reservation station release**" << endl;
        cout << "*approving commit*" << endl;
#endif
        tempMapEntry->readyToCommit = true;
#ifdef PRINT_DBG
        cout << "After approving commit, map entry: " << endl;
#endif
        pending_instructions.entries[tempMapEntry->instrWindowIndex].wr =
            clock_cycle;
        rob.entries[tempMapEntry->robIndex].state = WRITE_RESULT;
#ifdef PRINT_DBG
        print_map_entry(key);
#endif
      }
    }
    it++;
  }
#ifdef PRINT_DBG
  if(noInstructionsReadyToWrite){
    cout << "No instructions ready to write." << endl;
  }
  #endif

}

void sim_ooo::update_reservation_stations(unsigned thisResStationIndex){
     //get map entry of completed instruction
     //get value of completed instruction

  unsigned tempDestination = reservation_stations.entries[thisResStationIndex].destination;

  unsigned valueFromRob = rob.entries[tempDestination].value;
     //print output
     //from reservation station
     //print_write_results(1, thisPC, valueToWrite,tempDestination,0);
     map_entry_t* tempMapEntry;
     for(unsigned i = 0; i<reservation_stations.num_entries; i++){
       if(reservation_stations.entries[i].tag1 == tempDestination){
         reservation_stations.entries[i].tag1 = UNDEFINED;
         reservation_stations.entries[i].value1 = valueFromRob;
       }
       if (reservation_stations.entries[i].tag2 == tempDestination) {
         reservation_stations.entries[i].tag2 = UNDEFINED;
         reservation_stations.entries[i].value2 = valueFromRob;
       }
    }

}


void sim_ooo::clear_reservation_station(unsigned thisReservationStation){
      reservation_stations.entries[thisReservationStation].pc = UNDEFINED;
      reservation_stations.entries[thisReservationStation].address = UNDEFINED;
      reservation_stations.entries[thisReservationStation].destination = UNDEFINED;
      reservation_stations.entries[thisReservationStation].value1 = UNDEFINED;
      reservation_stations.entries[thisReservationStation].value2 = UNDEFINED;
      reservation_stations.entries[thisReservationStation].tag1 = UNDEFINED;
      reservation_stations.entries[thisReservationStation].tag2 = UNDEFINED;
}

void print_write_status(bool writeStatus){
  if(writeStatus)
    cout << "ready to write" << endl;
  else
    cout << "not ready to write" << endl;
}

 /////////////////////////////
 // COMMIT FUNCTIONS        //
 /////////////////////////////
void sim_ooo::commit(){
  //print_commit_init();
  commit_find();
}


void sim_ooo::commit_find(){
//access queue
//get info from queue
//map iterator
std::map<unsigned,map_entry_t>::iterator it;
it = instruction_map.begin();

unsigned thisDestination = UNDEFINED;
unsigned valueToBeCommitted = UNDEFINED;
unsigned frontOfrobQueue = robq.front();
unsigned queueToKey = frontOfrobQueue*4;
static unsigned storeLatency = UNDEFINED;
bool fpCommit = false;
opcode_t thisOpcode;
if(eop_issued&&robq.empty()){
#ifdef PRINT_DBG
  cout << "Program complete " <<endl;
  #endif
  programComplete=true;
}
unsigned mapCount = instruction_map.count(queueToKey);
if(it==instruction_map.end()){

#ifdef PRINT_DBG
  cout << "This map entry doesnt exist." << endl;
  cout << "Value returned by map.count(): ";
  cout << mapCount << endl;
  #endif
}
if(it!=instruction_map.end()){
  map_entry_t tempMapEntry = it->second;
  thisOpcode = tempMapEntry.instructionOpcode;
#ifdef PRINT_DBG
  cout << "Map entry does exist." << endl;
  cout << "Map entry: " << endl;
  print_string_opcode(tempMapEntry.instructionOpcode);
  print_map_entry(tempMapEntry);
  #endif
  // check if in order commit is ready
  if (tempMapEntry.readyToCommit) {
    if((thisOpcode!=SW)&&(thisOpcode!=SWS)){
#ifdef PRINT_DBG
    cout << "****THIS INSTRUCTION IS READY TO COMMIT****" << endl;
    // remove from commit queue
    cout << "***removing this instruction from queue***" << endl;
  #endif
    pending_instructions.entries[tempMapEntry.instrWindowIndex].commit =
        clock_cycle;
    robq.pop();
    thisDestination = rob.entries[tempMapEntry.robIndex].destination;
    if (is_fp_instruction(tempMapEntry.instructionOpcode)) {
      fpCommit = true;
      thisDestination -= 32;
#ifdef PRINT_DBG
      cout << "This instruction destination is: F";
      #endif
    } else {
#ifdef PRINT_DBG
      cout << "This instruction destination is: R";
      #endif
    }
    #ifdef PRINT_DBG
    cout << thisDestination << endl;
    #endif
    valueToBeCommitted = rob.entries[tempMapEntry.robIndex].value;
    if (!is_conditional(tempMapEntry.instructionOpcode)) {
      commit_commit(fpCommit, thisDestination, valueToBeCommitted, tempMapEntry.robIndex);
    }
    else if(is_conditional(tempMapEntry.instructionOpcode)){
      doBranchCommit= true;
      branchDestination = thisDestination;
      branchValue = valueToBeCommitted;
    }
    else{
    }
    commit_to_log(pending_instructions.entries[tempMapEntry.instrWindowIndex]);
#ifdef PRINT_DBG
    print_log();
    #endif
#ifdef PRINT_DBG
    cout << "Adding to ROB clear queue" << endl;
      #endif
    robs_to_clear.push(tempMapEntry.robIndex);
#ifdef PRINT_DBG
    cout << "Adding to instruction window clear queue" << endl;
      #endif
    instr_windows_to_clear.push(tempMapEntry.instrWindowIndex);
#ifdef PRINT_DBG
    cout << "Removing from instruction map" << endl;
      #endif
    instruction_map.erase(it);
    instructions_executed++;
  }
    else{
    storeExecutionUnitNumber = tempMapEntry.executionUnitNumber;
    pending_instructions.entries[tempMapEntry.instrWindowIndex].commit =
        clock_cycle;
    storeCommitLock = true;
    storeCommitTime = exec_units[storeExecutionUnitNumber].latency;
    storeLatency = storeCommitTime;
    exec_units[storeExecutionUnitNumber].busy = 0;
    exec_units[storeExecutionUnitNumber].pc = UNDEFINED;
    storeValue = rob.entries[tempMapEntry.robIndex].value;
    storeDestination = rob.entries[tempMapEntry.robIndex].destination;
#ifdef STORE_DBG
    cout << "Committing store instruction 1 at clock cycle: " << clock_cycle << endl;
    cout << "Showing initial store latency: " << storeCommitTime<< endl;
    cout << "Execution unit number: " << storeExecutionUnitNumber << endl;
    cout << "Time until lock lifted: " << storeCommitTime << endl;
#endif
    robs_to_clear.push(tempMapEntry.robIndex);
#ifdef STORE_DBG
    cout << "Adding to instruction window clear queue" << endl;
#endif
    instr_windows_to_clear.push(tempMapEntry.instrWindowIndex);
#ifdef STORE_DBG
    cout << "Removing from instruction map" << endl;
#endif
    commit_to_log(pending_instructions.entries[tempMapEntry.instrWindowIndex]);
    robq.pop();
    instruction_map.erase(it);
    instructions_executed++;
    store_commit();
  }
  }
  // if in order commit is ready
}
}
void sim_ooo::store_commit(){
  static bool firstRun = true;
  static unsigned frontOfrobQueue;
  static unsigned queueToKey;
  static unsigned execUnitNumber;
  std::map<unsigned, map_entry_t>::iterator it;
#ifdef STORE_DBG
        cout << "About to take action" << endl;
        cout << firstRun << endl;
#endif
   storeCommitTime--;
  if(storeCommitTime==1){
#ifdef STORE_DBG
        cout << "About to take action" << endl;
#endif
        take_store_action(storeValue, storeDestination);
#ifdef STORE_DBG
        cout << "Took action" << endl;
#endif
        storeCommitLock = false;
  }
}
void sim_ooo::branch_commit(unsigned thisDestination, unsigned thisValueToBeCommitted){
#ifdef BRANCH_EOP_DBG
      cout << "Branch info(destination,value) " << endl;
      cout << thisDestination << endl;
      cout << "PC determined by branch instruction: " << thisValueToBeCommitted << endl;
      cout << "Branch predicted PC: " << branchPredictedValue << endl;
#endif
      if(eop_issued){
#ifdef BRANCH_EOP_DBG
        cout << "EOP has been triggered and is in the instruction q" << endl;
      #endif
        eop_issued = false;
      }
      if(branchPredictedValue!=thisValueToBeCommitted){
#ifdef BRANCH_DBG
        cout << "test" << endl;
#endif
        unsigned removalCount = 0;
        if(!branch_instruction_map_keys.empty()){
          while(!branch_instruction_map_keys.empty()){
            removalCount++;
#ifdef BRANCH_DBG
            cout << "Must clean ROB and Res. Stations" << endl;
            cout << "Before deletions:" << endl;
            print_rob();
            print_reservation_stations();
#endif
            unsigned tempMapKey = branch_instruction_map_keys.front();
            map_entry_t tempMapEntry = instruction_map.find(tempMapKey)->second;
#ifdef BRANCH_DBG
            cout << "First entry to be cleared: " << tempMapKey << endl;
            cout << "Checking if in execution." << endl;
#endif
            unsigned execUnitNumber = tempMapEntry.executionUnitNumber;
            if (execUnitNumber != UNDEFINED) {
#ifdef BRANCH_DBG
              cout << "It is being executed. Must remove from execution unit"
                   << endl;
#endif
              exec_units[execUnitNumber].busy = 0;
              exec_units[execUnitNumber].pc = UNDEFINED;
            }
#ifdef BRANCH_DBG
            cout << "Location reservation station entry." << endl;
#endif
            unsigned resStationIndex = tempMapEntry.resStationIndex;
            clear_reservation_station(resStationIndex);
#ifdef BRANCH_DBG
            cout << "Locating ROB entry." << endl;
#endif
            unsigned robIndex = tempMapEntry.robIndex;
            //find name in registers
            for(unsigned i = 0; i< NUM_GP_REGISTERS; i++){
              if(float_gp[i].name == robIndex){
#ifdef REG_DBG
                cout << "Found tag at float entry " << i <<endl;
                #endif
                float_gp[i].name = UNDEFINED;
                float_gp[i].busy = false;
              }
              if(int_gp[i].name == robIndex){
#ifdef REG_DBG
                cout << "Found tag at int entry " << i <<endl;
                #endif
                int_gp[i].name=UNDEFINED;
                int_gp[i].busy=false;
              }
            }
            clear_rob_entry(robIndex);
#ifdef BRANCH_DBG
            cout << "Locating instruction window entry." << endl;
#endif
            unsigned instrWindowIndex = tempMapEntry.instrWindowIndex;
#ifdef BRANCH_DBG
            cout << "Committing to log" << endl;
#endif
            commit_to_log(pending_instructions.entries[instrWindowIndex]);
#ifdef BRANCH_DBG
            cout << "Removing from instr window index" << endl;
#endif
            instruction_window_remove(instrWindowIndex);
#ifdef BRANCH_DBG
            cout << "After deletions:" << endl;
            print_rob();
#endif
#ifdef BRANCH_DBG
            print_reservation_stations();
            print_map_entry(tempMapEntry);
#endif
#ifdef BRANCH_DBG
            cout << "Removing instruction map entry." << endl;
#endif
            instruction_map.erase(tempMapKey);
#ifdef BRANCH_DBG
            cout << "Moving to next erasable entry" << endl;
#endif
            branch_instruction_map_keys.pop();
          }
#ifdef BRANCH_EOP_DBG
          cout << "Number of instructions removed: " << removalCount << endl;
          #endif
          for(unsigned i = 0; i<removalCount; i++){
            robq.pop();
          }
        }
#ifdef BRANCH_DBG
        cout << "Changing PC to: " << thisValueToBeCommitted/4 << endl;
#endif
        branchCorrectedValue = thisValueToBeCommitted/4;
#ifdef BRANCH_DBG
        cout <<  "Clearing branch trigger" << endl;
#endif
        branchTriggerCleared=true;
      }

}
void sim_ooo::commit_commit(bool isFloat, unsigned thisRegister,
                            unsigned thisValue, unsigned thisRobIndex) {
#ifdef PRINT_DBG
  cout << "Updating register value" << endl;
#endif

  if (isFloat) {
    float fpValue = unsigned2float(thisValue);
#ifdef REG_DBG
    cout << "This register: " << thisRegister << endl;
    cout << "Name: " << float_gp[thisRegister].name << endl;
    cout << "Coming from rob index: " << thisRobIndex << endl;;
    #endif
    float_gp[thisRegister].value = fpValue;
    if(thisRobIndex==float_gp[thisRegister].name){
      float_gp[thisRegister].busy = false;
      float_gp[thisRegister].name = UNDEFINED;
    }
    // for(unsigned i = 0; i< rob.num_entries; i++){
    //   if(rob.entries[i].destination==(thisRegister+32)){
    //     float_gp[thisRegister].name=i;
    //     float_gp[thisRegister].busy = true;
    //   }
    // }
  } else {
    int_gp[thisRegister].value = thisValue;
    if(thisRobIndex==int_gp[thisRegister].name){
      int_gp[thisRegister].busy = false;
      int_gp[thisRegister].name = UNDEFINED;
    }
    // for(unsigned i = 0; i< rob.num_entries; i++){
    //   if(rob.entries[i].destination==thisRegister){
    //     int_gp[thisRegister].name=i;
    //     int_gp[thisRegister].busy = true;
    //   }
    // }
  }
#ifdef PRINT_DBG
  cout << "Register value has been updated" << endl;
#endif
}

void print_commit_message(){
       #ifdef PRINT_DBG
  cout << "***PREPARING TO SEARCH FOR COMMITABLE MESSAGES***" << endl;
#endif

}

void sim_ooo::clear_rob_entry(unsigned int thisRobEntry){
  rob.entries[thisRobEntry].pc = UNDEFINED;
  rob.entries[thisRobEntry].ready = false;
  rob.entries[thisRobEntry].empty = true;
  rob.entries[thisRobEntry].value = UNDEFINED;
  rob.entries[thisRobEntry].destination = UNDEFINED;
}

  /////////////////////////////
  // POST PROCESS FUNCTIONS  //
  /////////////////////////////

void sim_ooo::post_process(){
  unsigned thisEntry;
  while (!res_stations_to_update.empty()) {
    thisEntry = res_stations_to_update.front();
    update_reservation_stations(thisEntry);
    res_stations_to_update.pop();
  }
  while(!robs_to_clear.empty()){
    thisEntry = robs_to_clear.front();
    clear_rob_entry(thisEntry);
    robs_to_clear.pop();
  }
  while(!res_stations_to_clear.empty()){
    thisEntry = res_stations_to_clear.front();
    clear_reservation_station(thisEntry);
    res_stations_to_clear.pop();
  }
  while(!instr_windows_to_clear.empty()){
    thisEntry = instr_windows_to_clear.front();
    instruction_window_remove(thisEntry);
    instr_windows_to_clear.pop();
  }
  if(doBranchCommit){
    branch_commit(branchDestination,branchValue);
    doBranchCommit = false;
    branchDestination = UNDEFINED;
    branchValue = UNDEFINED;
    branchRobCorrect = true;
  }
}
