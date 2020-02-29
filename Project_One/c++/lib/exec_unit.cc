#include "../include/exec_unit.h"

exec_unit::exec_unit(unsigned unit_latency, pipe_stage_three temp_reg){
  reset();
  latency = unit_latency + 1;
  num_cycles = 0;
  UNIT_REG = temp_reg;
}
void exec_unit::run_cycle(){
  if(num_cycles < latency){
    num_cycles++;
  }
}
bool exec_unit::done(){
  return num_cycles >= latency;
}
void exec_unit::reset(){
  UNIT_REG.IR.opcode    = NOP;
  UNIT_REG.IR.src1      = UNDEFINED;
  UNIT_REG.IR.src2      = UNDEFINED;
  UNIT_REG.IR.dest      = UNDEFINED;
  UNIT_REG.IR.immediate = UNDEFINED;
  UNIT_REG.IR.label     = "";
  UNIT_REG.B            = UNDEFINED;
  UNIT_REG.condition    = UNDEFINED;
  UNIT_REG.ALU_out      = UNDEFINED;
}
pipe_stage_three exec_unit::getRegister(){
  return UNIT_REG;
}
unsigned exec_unit::get_remaining_cycles(){
  return latency - num_cycles;
}

void exec_unit::print_exec_unit(){
  printf("------------------------------\n");
  printf("EXECUTION UNIT:\n");
  printf("EXE current opcode: %d\n", UNIT_REG.IR.opcode);
  printf("EXE SRC 1:          %d\n", UNIT_REG.IR.src1);
  printf("EXE SRC 2:          %d\n", UNIT_REG.IR.src2);
  printf("EXE DEST:           %d\n", UNIT_REG.IR.dest);
  printf("EXE IMM:            %d\n", UNIT_REG.IR.immediate);
  printf("EXE B:              %d\n", UNIT_REG.B);
  printf("EXE LATENCY:        %d\n", latency);
  printf("EXE NUMB CYCLES     %d\n", num_cycles);
  printf("UNIT TYPE:          %d\n", UNIT_REG.IR.type);
  printf("------------------------------\n");
}
