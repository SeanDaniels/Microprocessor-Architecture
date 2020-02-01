#include "sim_pipe_fp.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
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
static const char *unit_names[4]={"INTEGER", "ADDER", "MULTIPLIER", "DIVIDER"};

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
	num_units = 0;
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
	for (unsigned i=0; i<instances; i++){
		exec_units[num_units].type = exec_unit;
		exec_units[num_units].latency = latency;
		exec_units[num_units].busy = 0;
		exec_units[num_units].instruction.opcode = NOP;
		num_units++;
	}
}

/* returns a free unit for that particular operation or UNDEFINED if no unit is currently available */
unsigned sim_pipe_fp::get_free_unit(opcode_t opcode){
	if (num_units == 0){
		cout << "ERROR:: simulator does not have any execution units!\n";
		exit(-1);
	}
	for (unsigned u=0; u<num_units; u++){
		switch(opcode){
			//Integer unit
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
				if (exec_units[u].type==INTEGER && exec_units[u].busy==0) return u;
				break;
			// FP adder
			case ADDS:
			case SUBS:
				if (exec_units[u].type==ADDER && exec_units[u].busy==0) return u;
				break;
			// Multiplier
			case MULTS:
				if (exec_units[u].type==MULTIPLIER && exec_units[u].busy==0) return u;
				break;
			// Divider
			case DIVS:
				if (exec_units[u].type==DIVIDER && exec_units[u].busy==0) return u;
				break;
			default:
				cout << "ERROR:: operations not requiring exec unit!\n";
				exit(-1);
		}
	}
	return UNDEFINED;
}

/* decrease the amount of clock cycles during which the functional unit will be busy - to be called at each clock cycle  */
void sim_pipe_fp::decrement_units_busy_time(){
	for (unsigned u=0; u<num_units; u++){
		if (exec_units[u].busy > 0) exec_units[u].busy --;
	}
}


/* prints out the status of the functional units */
void sim_pipe_fp::debug_units(){
	for (unsigned u=0; u<num_units; u++){
		cout << " -- unit " << unit_names[exec_units[u].type] << " latency=" << exec_units[u].latency << " busy=" << exec_units[u].busy <<
			" instruction=" << instr_names[exec_units[u].instruction.opcode] << endl;
	}
}

/* ========= end primitives related to functional units ===============*/


/* ========================parser ==================================== */

void sim_pipe_fp::load_program(const char *filename, unsigned base_address){

   /* initializing the base instruction address */
   instr_base_address = base_address;

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

	// set the instruction field
	char *str = const_cast<char*>(line.c_str());

  	// tokenize the instruction
	char *token = strtok (str," \t");
	map<string, opcode_t>::iterator search = opcodes.find(token);
        if (search == opcodes.end()){
		// this is a label for a branch - extract it and save it in the labels map
		string label = string(token).substr(0, string(token).length() - 1);
		labels[label]=instruction_nr;
                // move to next token, which must be the instruction opcode
		token = strtok (NULL, " \t");
		search = opcodes.find(token);
		if (search == opcodes.end()) cout << "ERROR: invalid opcode: " << token << " !" << endl;
	}
	instr_memory[instruction_nr].opcode = search->second;

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

	/* complete the reset function here */

}

//return value of special purpose register
unsigned sim_pipe_fp::get_sp_register(sp_register_t reg, stage_t s){
	return 0; // please modify
}

int sim_pipe_fp::get_int_register(unsigned reg){
	return 0; // please modify
}

void sim_pipe_fp::set_int_register(unsigned reg, int value){
}

float sim_pipe_fp::get_fp_register(unsigned reg){
	return 0.0; // please modify
}

void sim_pipe_fp::set_fp_register(unsigned reg, float value){
}


float sim_pipe_fp::get_IPC(){
	return 0.0; // please modify
}

unsigned sim_pipe_fp::get_instructions_executed(){
	return 0; // please modify
}

unsigned sim_pipe_fp::get_clock_cycles(){
	return 0; // please modify
}

unsigned sim_pipe_fp::get_stalls(){
	return 0; // please modify
}
