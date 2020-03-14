#include "../include/sim_pipe_fp.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <iomanip>
#include <map>


//NOTE: structural hazards on MEM/WB stage not handled
//====================================================

//#define DEBUG
//#define DEBUG_MEMORY

using namespace std;

//used for debugging purposes
static const char *reg_names[NUM_SP_REGISTERS] = {"PC", "NPC", "IR", "A", "B", "IMM", "COND", "ALU_OUTPUT", "LMD"};
static const char *stage_names[NUM_STAGES] = {"IF", "ID", "EX", "MEM", "WB"};
static const char *instr_names[NUM_OPCODES] = {"LW", "SW", "ADD", "ADDI", "SUB", "SUBI", "XOR", "BEQZ", "BNEZ", "BLTZ", "BGTZ", "BLEZ", "BGEZ", "JUMP", "EOP", "NOP", "LWS", "SWS", "ADDS", "SUBS", "MULTS", "DIVS"};



/* =============================================================

   HELPER FUNCTIONS

   ============================================================= */

/* convert a float into an unsigned */
inline unsigned float2unsigned(float value){
	unsigned result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert an unsigned into a float */
inline float unsigned2float(unsigned value){
	float result;
	memcpy(&result, &value, sizeof value);
	return result;
}

/* convert integer into array of unsigned char - little indian */
inline void unsigned2char(unsigned value, unsigned char *buffer){
        buffer[0] = value & 0xFF;
        buffer[1] = (value >> 8) & 0xFF;
        buffer[2] = (value >> 16) & 0xFF;
        buffer[3] = (value >> 24) & 0xFF;
}

/* convert array of char into integer - little indian */
inline unsigned char2unsigned(unsigned char *buffer){
  return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}

/* the following functions return the kind of the considered opcode */

bool is_branch(opcode_t opcode){
	return (opcode == BEQZ || opcode == BNEZ || opcode == BLTZ || opcode == BLEZ || opcode == BGTZ || opcode == BGEZ || opcode == JUMP);
}

bool is_memory(opcode_t opcode){
  return (opcode == LW || opcode == SW || opcode == LWS || opcode == SWS);
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
bool is_int_r(opcode_t opcode){
  return (opcode == ADD || opcode == SUB || opcode == XOR);
}

bool is_int_imm(opcode_t opcode){
  return (opcode == ADDI || opcode == SUBI);
}

bool is_int_alu(opcode_t opcode){
  return (is_int_r(opcode) || is_int_imm(opcode));
}

bool is_fp_alu(opcode_t opcode){
  return (opcode == ADDS || opcode == SUBS || opcode == MULTS || opcode == DIVS);
}

/* implements the ALU operations */
unsigned alu(unsigned opcode, unsigned a, unsigned b, unsigned imm, unsigned npc){
	switch(opcode){
			case ADD:
				return (a+b);
			case ADDI:
				return(a+imm);
			case SUB:
				return(a-b);
			case SUBI:
				return(a-imm);
			case XOR:
				return(a ^ b);
			case LW:
			case SW:
			case LWS:
			case SWS:
				return(a + imm);
			case BEQZ:
			case BNEZ:
			case BGTZ:
			case BGEZ:
			case BLTZ:
			case BLEZ:
			case JUMP:
				return(npc+imm);
			case ADDS:
				return(float2unsigned(unsigned2float(a)+unsigned2float(b)));
				break;
			case SUBS:
				return(float2unsigned(unsigned2float(a)-unsigned2float(b)));
				break;
			case MULTS:
				return(float2unsigned(unsigned2float(a)*unsigned2float(b)));
				break;
			case DIVS:
				return(float2unsigned(unsigned2float(a)/unsigned2float(b)));
				break;
			default:
				return (-1);
	}
}

/* =============================================================

   CODE PROVIDED - NO NEED TO MODIFY FUNCTIONS BELOW

   ============================================================= */

/* ============== primitives to allocate/free the simulator ================== */

sim_pipe_fp::sim_pipe_fp(unsigned mem_size, unsigned mem_latency){
	data_memory_size = mem_size;
	data_memory_latency = mem_latency;
	data_memory = new unsigned char[data_memory_size];
  sp_register_bank = new sp_registers;
  STALL.opcode = NOP;
  STALL.src1 = UNDEFINED;
  STALL.src2 = UNDEFINED;
  STALL.dest = UNDEFINED;
  STALL.immediate = UNDEFINED;
  STALL.label = "";
	reset();
}

sim_pipe_fp::~sim_pipe_fp(){
	delete [] data_memory;
}

/* =============   primitives to print out the content of the memory & registers and for writing to memory ============== */

void sim_pipe_fp::print_memory(unsigned start_address, unsigned end_address){
	cout << "data_memory[0x" << hex << setw(8) << setfill('0') << start_address << ":0x" << hex << setw(8) << setfill('0') <<  end_address << "]" << endl;
	for (unsigned i=start_address; i<end_address; i++){
		if (i%4 == 0) cout << "0x" << hex << setw(8) << setfill('0') << i << ": ";
		cout << hex << setw(2) << setfill('0') << int(data_memory[i]) << " ";
		if (i%4 == 3){
#ifdef DEBUG_MEMORY
			unsigned u = char2unsigned(&data_memory[i-3]);
			cout << " - unsigned=" << u << " - float=" << unsigned2float(u);
#endif
			cout << endl;
		}
	}
}

void sim_pipe_fp::write_memory(unsigned address, unsigned value){
	unsigned2char(value,data_memory+address);
}

unsigned sim_pipe_fp::load_memory(unsigned address){
  	unsigned d;
  	memcpy(&d, data_memory + address, sizeof d);
  	return d;
}

void sim_pipe_fp::print_registers(){
        cout << "Special purpose registers:" << endl;
        unsigned i, s;
        for (s=0; s<NUM_STAGES; s++){
                cout << "Stage: " << stage_names[s] << endl;
                for (i=0; i< NUM_SP_REGISTERS; i++)
                        if ((sp_register_t)i != IR && (sp_register_t)i != COND && get_sp_register((sp_register_t)i, (stage_t)s)!=UNDEFINED) cout << reg_names[i] << " = " << dec <<  get_sp_register((sp_register_t)i, (stage_t)s) << hex << " / 0x" << get_sp_register((sp_register_t)i, (stage_t)s) << endl;
        }
        cout << "General purpose registers:" << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++)
                if (get_int_register(i)!=(int)UNDEFINED) cout << "R" << dec << i << " = " << get_int_register(i) << hex << " / 0x" << get_int_register(i) << endl;
        for (i=0; i< NUM_GP_REGISTERS; i++)
                if (get_fp_register(i)!=UNDEFINED) cout << "F" << dec << i << " = " << get_fp_register(i) << hex << " / 0x" << float2unsigned(get_fp_register(i)) << endl;
}


/* =============   primitives related to the functional units ============== */

/* initializes an execution unit */
void sim_pipe_fp::init_exec_unit(exe_unit_t exec_unit, unsigned latency, unsigned instances){
	switch(exec_unit){
    case INTEGER:
      max_integer_units = instances;
      integer_unit_latency = latency;
    break;
    case ADDER:
      max_adder_units = instances;
      adder_unit_latency = latency;
    break;
    case MULTIPLIER:
      max_mult_units = instances;
      mult_unit_latency = latency;
    break;
    case DIVIDER:
      max_div_units = instances;
      div_unit_latency = latency;
    break;
    default:
    break;
  }
}

/* ========= end primitives related to functional units ===============*/


/* ========================parser ==================================== */

void sim_pipe_fp::load_program(const char *filename, unsigned base_address){

   /* initializing the base instruction address */
   instr_base_address = base_address;
   sp_register_bank->set_IF(instr_base_address);
   /* creating a map with the valid opcodes and with the valid labels */
   map<string, opcode_t> opcodes; //for opcodes
   map<string, unsigned> labels;  //for branches
   for (int i=0; i<NUM_OPCODES; i++)
	 opcodes[string(instr_names[i])]=(opcode_t)i;

   /* opening the assembly file */
   ifstream fin(filename, ios::in | ios::binary);
   if (!fin.is_open()) {
      cerr << "error: open file " << filename << " failed!" << endl;
      exit(-1);
   }

   /* parsing the assembly file line by line */
   string line;
   unsigned instruction_nr = 0;
   while (getline(fin,line)){
	instr_memory[instruction_nr].instruction = line;

	// set the instruction field
	char *str = const_cast<char*>(line.c_str());

  	// tokenize the instruction
	char *token = strtok (str," \t");
	map<string, opcode_t>::iterator search = opcodes.find(token);
        if (search == opcodes.end()){
		// this is a label for a branch - extract it and save it in the labels map
		string label = string(token).substr(0, string(token).length() - 1);
		instr_memory[instruction_nr].instruction.erase(0,label.length()+1);
		labels[label]=instruction_nr;
                // move to next token, which must be the instruction opcode
		token = strtok (NULL, " \t");
		search = opcodes.find(token);
		if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
	}
	instr_memory[instruction_nr].opcode = search->second;
/*
	while(instr_memory[instruction_nr].instruction.at(0)=='\t' || instr_memory[instruction_nr].instruction.at(0)==' ')
                instr_memory[instruction_nr].instruction.erase(0,1);
        replace( instr_memory[instruction_nr].instruction.begin(), instr_memory[instruction_nr].instruction.end(), '\t', ' ');
*/
	//reading remaining parameters
	char *par1;
	char *par2;
	char *par3;
	switch(instr_memory[instruction_nr].opcode){
		case ADD:
		case SUB:
		case XOR:
		case ADDS:
		case SUBS:
		case MULTS:
		case DIVS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "RF"));
			instr_memory[instruction_nr].src2 = atoi(strtok(par3, "RF"));
			break;
		case ADDI:
		case SUBI:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			par3 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].src1 = atoi(strtok(par2, "R"));
			instr_memory[instruction_nr].immediate = strtoul (par3, NULL, 0);
			break;
		case LW:
		case LWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].dest = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src1 = atoi(strtok(NULL, "R"));
			break;
		case SW:
		case SWS:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "RF"));
			instr_memory[instruction_nr].immediate = strtoul(strtok(par2, "()"), NULL, 0);
			instr_memory[instruction_nr].src2 = atoi(strtok(NULL, "R"));
			break;
		case BEQZ:
		case BNEZ:
		case BLTZ:
		case BGTZ:
		case BLEZ:
		case BGEZ:
			par1 = strtok (NULL, " \t");
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].src1 = atoi(strtok(par1, "R"));
			instr_memory[instruction_nr].label = par2;
			break;
		case JUMP:
			par2 = strtok (NULL, " \t");
			instr_memory[instruction_nr].label = par2;
		default:
			break;

	}

	/* increment instruction number before moving to next line */
	instruction_nr++;
   }
   //reconstructing the labels of the branch operations
   int i = 0;
   while(true){
   	instruction_t instr = instr_memory[i];
	if (instr.opcode == EOP) break;
	if (instr.opcode == BLTZ || instr.opcode == BNEZ ||
            instr.opcode == BGTZ || instr.opcode == BEQZ ||
            instr.opcode == BGEZ || instr.opcode == BLEZ ||
            instr.opcode == JUMP
	 ){
		instr_memory[i].immediate = (labels[instr.label] - i - 1) << 2;
	}
        i++;
   }

}

/* =============================================================

   CODE TO BE COMPLETED

   ============================================================= */

/* simulator */
void sim_pipe_fp::run(unsigned cycles){
  if(cycles > 0){
    while(cycles > 0){
      cycle_clock();
      cycles--;
    }
  }
  else{
    while(!EOP_flag){
      cycle_clock();
    }
  }

}
void sim_pipe_fp::cycle_clock(){
//sp_register_bank->print_sp_registers();
  run_WB();
  run_MEM();
if(!SH_MEM_stall){
  run_EXE();
  run_ID();
  if(!DH_RAW_stall && !DH_WAW_stall && !SH_EXE_stall){
    run_IF();
  }
}
if(DH_RAW_stall || DH_WAW_stall || SH_EXE_stall ||  SH_MEM_stall){
  num_stalls++;
}
  DH_RAW_stall = 0;
  DH_WAW_stall = 0;
  SH_EXE_stall = 0;
  num_cycles++;
}
void sim_pipe_fp::run_IF(){
  instruction_t next_instruction = STALL;
  unsigned pc, npc;
  pc = get_sp_register(PC, IF);
  instruction_t temp = get_IR(WB);
  if(temp.opcode > XOR && temp.opcode < EOP ){
    CH_stall = 0;
    num_control_stalls++;
    if(get_sp_register(COND,WB)){
      pc = get_sp_register(ALU_OUTPUT,WB);
    }
  }
  next_instruction = instr_memory[(pc - instr_base_address) / 4];
  npc = pc + 4;
  if(CH_stall){
    npc = npc - 4;
    next_instruction = STALL;
  }
  else if(next_instruction.opcode == EOP){
    npc = npc - 4;
  }
  sp_register_bank->set_IF(npc);
  if(CH_stall){
    npc = UNDEFINED;
  }
  sp_register_bank->set_IFID(npc, next_instruction);
  if(next_instruction.opcode > XOR && next_instruction.opcode < EOP){
    CH_stall = 1;
    CH_stalls_inserted = 1;
    num_control_stalls++;
  }



}
void sim_pipe_fp::run_ID(){
  unsigned temp_A, temp_B, npc, immediate;
  instruction_t ID_instruction;
  ID_instruction = get_IR(ID);
  switch(ID_instruction.opcode){
    case ADD:
    case SUB:
    case XOR:
      temp_A = get_int_register(ID_instruction.src1);
      temp_B = get_int_register(ID_instruction.src2);
      ID_instruction.immediate = UNDEFINED;
      ID_instruction.type = INTEGER;
      break;
    case ADDI:
    case SUBI:
      temp_A = get_int_register(ID_instruction.src1);
      temp_B = UNDEFINED;
      ID_instruction.type = INTEGER;
      break;
    case LW:
    case LWS:
      temp_A = get_int_register(ID_instruction.src1);
      temp_B = UNDEFINED;
      ID_instruction.type = INTEGER;
    break;
    case SW:
      temp_A = get_int_register(ID_instruction.src2);
      temp_B = get_int_register(ID_instruction.src1);
      ID_instruction.dest = UNDEFINED;
      ID_instruction.type = INTEGER;
    break;
    case SWS:
      temp_A = get_int_register(ID_instruction.src2);
      temp_B = float2unsigned(get_fp_register(ID_instruction.src1));
      ID_instruction.dest = UNDEFINED;
      ID_instruction.type = INTEGER;
      break;
    case BEQZ:
    case BNEZ:
    case BGTZ:
    case BGEZ:
    case BLTZ:
    case BLEZ:
    case JUMP:
      temp_A = get_int_register(ID_instruction.src1);
      temp_B = UNDEFINED;
      ID_instruction.dest = UNDEFINED;
      ID_instruction.type = INTEGER;
    break;
    case ADDS:
    case SUBS:
      ID_instruction.type = ADDER;
      temp_A = float2unsigned(get_fp_register(ID_instruction.src1));
      temp_B = float2unsigned(get_fp_register(ID_instruction.src2));
      ID_instruction.immediate = UNDEFINED;
      break;
    case MULTS:
      temp_A = float2unsigned(get_fp_register(ID_instruction.src1));
      temp_B = float2unsigned(get_fp_register(ID_instruction.src2));
      ID_instruction.immediate = UNDEFINED;
      ID_instruction.type = MULTIPLIER;
      break;
    case DIVS:
      temp_A = float2unsigned(get_fp_register(ID_instruction.src1));
      temp_B = float2unsigned(get_fp_register(ID_instruction.src2));
      ID_instruction.immediate = UNDEFINED;
      ID_instruction.type = DIVIDER;
      break;
    default:
      temp_A = UNDEFINED;
      temp_B = UNDEFINED;
      ID_instruction.immediate = UNDEFINED;
      break;
  }
  npc = get_sp_register(NPC,ID);
  immediate = ID_instruction.immediate;
  check_RAW_dataHazard();
  check_WAW_dataHazard(ID_instruction.dest, ID_instruction.type);
  if(!check_unit_free(ID_instruction.type)){
    SH_EXE_stall = 1;
  }
  if(DH_WAW_stall || DH_RAW_stall || SH_EXE_stall){
    temp_A = UNDEFINED;
    temp_B = UNDEFINED;
    immediate = UNDEFINED;
    npc = UNDEFINED;
    ID_instruction = STALL;
  }
  sp_register_bank->set_IDEX(temp_A, temp_B, immediate, npc, ID_instruction);
}
void sim_pipe_fp::run_EXE(){
  unsigned a  = get_sp_register(A,EXE);
  unsigned b = get_sp_register(B,EXE);
  unsigned npc = get_sp_register(NPC,EXE);
  instruction_t EXE_instruction = get_IR(EXE);
  if(EXE_instruction.opcode != NOP && EXE_instruction.opcode != EOP){
    pipe_stage_three temp_sp;
    temp_sp.ALU_out = alu(EXE_instruction.opcode, a,b, EXE_instruction.immediate, npc);
    temp_sp.B = b;
    temp_sp.condition = get_condition(EXE_instruction, a);
    temp_sp.IR = EXE_instruction;
    unsigned latency = get_unit_latency(EXE_instruction.type);
    incrementUnitCount(EXE_instruction.type);
    exec_unit* newUnit = new exec_unit(latency, temp_sp);
    execution_units.push_back(newUnit);
  }
  cycle_all_units();
  pipe_stage_three finished_unit;
  if(!get_finished_unit(finished_unit)){
    if(execution_units.empty() && EXE_instruction.opcode == EOP){
      finished_unit.IR = EXE_instruction;
    }else{
      finished_unit.IR = STALL;
    }
    finished_unit.ALU_out = UNDEFINED;
    finished_unit.B = UNDEFINED;
    finished_unit.condition = ZERO;
    finished_unit.WE = UNDEFINED;
  }else{
    decrementUnitCount(finished_unit.IR.type);
  }

  sp_register_bank->set_EXMEM(finished_unit.B, finished_unit.ALU_out, finished_unit.condition, finished_unit.IR);
}
void sim_pipe_fp::run_MEM(){
  //
  unsigned alu_temp, condition;
  //
  instruction_t temp_instruction;
  //
  unsigned memory = UNDEFINED;
  //
  static unsigned mem_cycles = 0;
  //
  condition = get_sp_register(COND,MEM);
  temp_instruction = get_IR(MEM);
  alu_temp = get_sp_register(ALU_OUTPUT, MEM);
  if(mem_cycles < data_memory_latency && is_memory(temp_instruction.opcode)){
    //
    SH_MEM_stall = 1;
    //
    sp_register_bank->set_WE(MEM, 0);
    //
    mem_cycles++;
    //
    memory = UNDEFINED;
    //
    condition = 0;
    //
    temp_instruction = STALL;
    //
    alu_temp = UNDEFINED;
  }else if(is_memory(temp_instruction.opcode)){
    mem_cycles = 0;
    SH_MEM_stall = 0;
    sp_register_bank->set_WE(MEM, 1);
    if(temp_instruction.opcode == LW || temp_instruction.opcode == SW){
      if(temp_instruction.opcode == LW){
        memory = load_memory(alu_temp);
      }else if(temp_instruction.opcode == SW){
        write_memory(alu_temp, get_sp_register(B, MEM));
      }
    }else if(temp_instruction.opcode == LWS || temp_instruction.opcode == SWS){
      if(temp_instruction.opcode == LWS){
        memory = load_memory(alu_temp);
      }else if(temp_instruction.opcode == SWS){
        write_memory(alu_temp, get_sp_register(B, MEM));
      }
    }
  }
  sp_register_bank->set_MEMWB(memory, condition, temp_instruction, alu_temp);
}
void sim_pipe_fp::run_WB(){
  instruction_t temp_instruction  = get_IR(WB);
  if(temp_instruction.opcode == EOP){
    EOP_flag = 1;
  }
  if( SW < temp_instruction.opcode && temp_instruction.opcode < BEQZ){
    set_int_register(temp_instruction.dest, get_sp_register(ALU_OUTPUT, WB));
  }else if( SWS < temp_instruction.opcode && temp_instruction.opcode < DIVS + 1){
    set_fp_register(temp_instruction.dest, unsigned2float(get_sp_register(ALU_OUTPUT, WB)));
  }else if(temp_instruction.opcode == LW){
    set_int_register(temp_instruction.dest, get_sp_register(LMD, WB));
  }else if(temp_instruction.opcode == LWS){
    set_fp_register(temp_instruction.dest, unsigned2float(get_sp_register(LMD, WB)));
  }
  if(temp_instruction.opcode != EOP && temp_instruction.opcode != NOP){
    num_instructions++;
  }
}
void sim_pipe_fp::cycle_all_units(){
  if(!execution_units.empty()){
    list<exec_unit*>::iterator it = execution_units.begin();
    while(it != execution_units.end()){
      (*it)->run_cycle();
      //(*it)->print_exec_unit();
      it++;
    }
  }

}
bool sim_pipe_fp::get_finished_unit(pipe_stage_three& finished_unit){
  if(!execution_units.empty()){
    list<exec_unit*>::iterator  it = execution_units.begin();
    while(it != execution_units.end()){
      if((*it)->done()){
        finished_unit = (*it)->getRegister();
        execution_units.remove(*it);
        return true;
      }else{
        it++;
      }
    }
  }
  return false;
}
unsigned sim_pipe_fp::get_unit_latency(exe_unit_t unit){
  switch(unit){
    case ADDER:
      return adder_unit_latency;
    break;
    case INTEGER:
      return integer_unit_latency;
    break;
    case MULTIPLIER:
      return mult_unit_latency;
    break;
    case DIVIDER:
      return div_unit_latency;
    break;
    default:
      return 0;
    break;
  }
}
void sim_pipe_fp::printunits(){
  printf("the number of max_div_units is %d\n", max_div_units);
  printf("the number of num_div_units is %d\n", num_div_units);
  printf("the div latency is %d\n", div_unit_latency);
  printf("the number of max_adder_units is %d\n", max_adder_units);
  printf("the number of num_adder_units is %d\n", num_adder_units);
  printf("the adder latency is %d\n", adder_unit_latency);
  printf("the number of max_integer_units is %d\n", max_integer_units);
  printf("the number of num_integer_units is %d\n", num_integer_units);
  printf("the integer latency is %d\n", integer_unit_latency);
  printf("the number of max_mult_units is %d\n", max_mult_units);
  printf("the number of num_mult_units is %d\n", num_mult_units);
  printf("the mult latency is %d\n", mult_unit_latency);
}
bool sim_pipe_fp::check_unit_free(exe_unit_t unit){
  switch(unit){
    case ADDER:
      return (max_adder_units > num_adder_units);
    break;
    case INTEGER:
      return (max_integer_units > num_integer_units);
    break;
    case MULTIPLIER:
      return (max_mult_units > num_mult_units);
    break;
    case DIVIDER:
      return (max_div_units > num_div_units);
    break;
    default:
      return 0;
    break;
  }
}
void sim_pipe_fp::incrementUnitCount(exe_unit_t unit){
  switch(unit){
    case ADDER:
      num_adder_units++;
    break;
    case INTEGER:
      num_integer_units++;
    break;
    case MULTIPLIER:
      num_mult_units++;
    break;
    case DIVIDER:
      num_div_units++;
    break;
    default:
    break;
  }
}
void sim_pipe_fp::decrementUnitCount(exe_unit_t unit){
  switch(unit){
    case ADDER:
      num_adder_units--;
    break;
    case INTEGER:
      num_integer_units--;
    break;
    case MULTIPLIER:
      num_mult_units--;
    break;
    case DIVIDER:
      num_div_units--;
    break;
    default:
    break;
  }
}
//reset the state of the sim_pipe_fpulator
void sim_pipe_fp::reset(){
	// init data memory
	for (unsigned i=0; i<data_memory_size; i++) data_memory[i]=0xFF;
	// init instruction memory
	for (int i=0; i<PROGRAM_SIZE;i++){
		instr_memory[i].opcode=(opcode_t)NOP;
		instr_memory[i].src1=UNDEFINED;
		instr_memory[i].src2=UNDEFINED;
		instr_memory[i].dest=UNDEFINED;
		instr_memory[i].immediate=UNDEFINED;
	}
  for( int i = 0; i < NUM_GP_REGISTERS; i++){
    int_gp_reg[i] = UNDEFINED;
    fp_gp_reg[i]  = float2unsigned(UNDEFINED);
  }
  for(unsigned i = 0; i < data_memory_size; i++){
    data_memory[i] = 0xFF;
  }
	/* complete the reset function here */
  max_integer_units = ZERO;
  num_integer_units = ZERO;
  integer_unit_latency = ZERO;
  max_adder_units = ZERO;
  num_adder_units = ZERO;
  adder_unit_latency = ZERO;
  max_mult_units = ZERO;
  num_mult_units = ZERO;
  mult_unit_latency = ZERO;
  max_div_units = ZERO;
  num_div_units = ZERO;
  div_unit_latency = ZERO;

  //STALL INSTRUCTION
  STALL.opcode = NOP; //opcode
  STALL.src1 = UNDEFINED; //first source register
  STALL.src2 = UNDEFINED; //second source register
  STALL.dest = UNDEFINED; //destination register
  STALL.immediate = UNDEFINED; //immediate field;

  //data structures
  execution_units = list<exec_unit*>();
  //stall flags
  SH_EXE_stall = 0;
  SH_MEM_stall = 0;
  DH_RAW_stall = 0;
  DH_WAW_stall = 0;
  CH_stalls_inserted = 0;
  EOP_flag = 0;
  //counters
  num_instructions = 0;
  num_stalls = 0;
  num_control_stalls = -1;
  num_cycles = -1;
}
//return value of special purpose register
unsigned sim_pipe_fp::get_sp_register(sp_register_t reg, stage_t s){
	switch(s){
    case IF:
      return sp_register_bank->get_IF(reg);
    break;
    case ID:
      return sp_register_bank->get_IFID(reg);
    break;
    case EXE:
      return sp_register_bank->get_IDEX(reg);
    break;
    case MEM:
      return sp_register_bank->get_EXMEM(reg);
    break;
    case WB:
      return sp_register_bank->get_MEMWB(reg);
    break;
    default:
      return UNDEFINED;
    break;
  }
}
//done
int sim_pipe_fp::get_int_register(unsigned reg){
	return int_gp_reg[reg]; // please modify
}
//done
void sim_pipe_fp::set_int_register(unsigned reg, int value){
  int_gp_reg[reg] = value;
}
//done
float sim_pipe_fp::get_fp_register(unsigned reg){
	return unsigned2float(fp_gp_reg[reg]);
}
//done
void sim_pipe_fp::set_fp_register(unsigned reg, float value){
  fp_gp_reg[reg] = float2unsigned(value);
}
//done
unsigned sim_pipe_fp::get_condition(instruction_t temp_instruction, unsigned temp_A){
  unsigned temp_cond;
  if(temp_A == UNDEFINED || temp_instruction.opcode < BEQZ || temp_instruction.opcode > JUMP){
    return 0;
  }else{
    int temp;
    temp = (int) temp_A;
    switch(temp_instruction.opcode){
      case BEQZ:
        temp_cond = (temp == 0);
      break;
      case BNEZ:
        temp_cond = (temp != 0);
      break;
      case BLTZ:
        temp_cond = (temp < 0);
      break;
      case BGTZ:
        temp_cond = (temp > 0);
      break;
      case BLEZ:
        temp_cond = (temp <= 0);
      break;
      case BGEZ:
          temp_cond = (temp >= 0);
      break;
      case JUMP:
        temp_cond = 0x00000001;
      break;
      default:
        temp_cond = 0x00000000;
      break;
    }
    return temp_cond;
  }
}
float sim_pipe_fp::get_IPC(){
	return ((float)num_instructions/(float)num_cycles); // please modify
}
//done
unsigned sim_pipe_fp::get_instructions_executed(){
	return num_instructions; // please modify
}
//done
unsigned sim_pipe_fp::get_clock_cycles(){
	return num_cycles; // please modify
}
unsigned sim_pipe_fp::get_stalls(){
  if(CH_stalls_inserted){
    return num_stalls + num_control_stalls;
  }else{
    return num_stalls;
  }
}

instruction_t sim_pipe_fp::get_IR(stage_t s){
  instruction_t temp;
  sp_register_bank->get_IR(s, temp);
  return temp;
}

void sim_pipe_fp::check_RAW_dataHazard(){
  instruction_t ID_instruction = get_IR(ID);
  instruction_t MEM_instruction = get_IR(MEM);
  instruction_t WB_instruction = get_IR(WB);
  unsigned ID_src1 = ID_instruction.src1;
  unsigned ID_src2 = ID_instruction.src2;
  if(ID_instruction.opcode != NOP && ID_instruction.opcode != EOP){
    if(((ID_src1 == MEM_instruction.dest || ID_src2 == MEM_instruction.dest) && MEM_instruction.dest != UNDEFINED) ||
       ((ID_src1 == WB_instruction.dest  || ID_src2 == WB_instruction.dest ) && WB_instruction.dest  != UNDEFINED))
    {
      if ((writes_to_FP(MEM_instruction.opcode) &&
           reads_from_FP(ID_instruction.opcode)) ||
          (writes_to_FP(WB_instruction.opcode) &&
           reads_from_FP(ID_instruction.opcode)) ||
          (writes_to_GP(MEM_instruction.opcode) &&
           reads_from_GP(ID_instruction.opcode)) ||
          (writes_to_GP(WB_instruction.opcode) &&
           reads_from_GP(ID_instruction.opcode))) {
        DH_RAW_stall = 1;
      }
    }
  }
  if(!execution_units.empty()){
    list<exec_unit*>::iterator  it = execution_units.begin();
    while(it != execution_units.end()){
      pipe_stage_three temp = (*it)->getRegister();
      if(((ID_src1 == temp.IR.dest || ID_src2 == temp.IR.dest) && temp.IR.dest != UNDEFINED)){
        if((writes_to_FP(temp.IR.opcode) && reads_from_FP(ID_instruction.opcode)) ||
            (writes_to_GP(temp.IR.opcode) && reads_from_GP(ID_instruction.opcode )) ){
              DH_RAW_stall = 1;
            }
      }
      it++;
    }
  }
}

void sim_pipe_fp::check_WAW_dataHazard(unsigned dest, exe_unit_t type){
  unsigned latency = get_unit_latency(type);
  if(!execution_units.empty()){
    list<exec_unit*>::iterator  it = execution_units.begin();
    while(it != execution_units.end()){
      unsigned temp_dest = (*it)->getRegister().IR.dest;
      if(dest == temp_dest && temp_dest != UNDEFINED){
        if(latency < (*it)->get_remaining_cycles()){
          DH_WAW_stall = 1;
        }
      }
      it++;
    }
  }
}
