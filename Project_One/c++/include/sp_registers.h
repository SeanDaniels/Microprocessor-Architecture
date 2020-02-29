#ifndef SP_REGISTERS_H
#define SP_REGISTERS_H
#include "definitions.h"

using namespace std;

class sp_registers{
  fetch_registers IF;
  pipe_stage_one IFID;
  pipe_stage_two IDEX;
  pipe_stage_three EXMEM;
  pipe_stage_four MEMWB;
public:
  sp_registers();
  void reset();
  //IF stage accessor methods
  unsigned get_IF(sp_register_t);
  void set_IF(unsigned);
  //ID stage accessor methods
  unsigned get_IFID(sp_register_t);
  void set_IFID(unsigned, instruction_t);
  //EXE stage accessor methods
  unsigned get_IDEX(sp_register_t);
  void set_IDEX(unsigned, unsigned, unsigned, unsigned, instruction_t);
  //MEM stage accessor methods
  unsigned get_EXMEM(sp_register_t);
  void set_EXMEM(unsigned, unsigned, unsigned, instruction_t);
  //WB stage accessor methods
  unsigned get_MEMWB(sp_register_t);
  void set_MEMWB(unsigned, unsigned, instruction_t, unsigned);

  string opcodeToString(opcode_t op);
  void print_sp_registers();

  bool get_IR(stage_t, instruction_t&);
  void toggle_WE(stage_t);
  void set_WE(stage_t stage, unsigned val);
};

#endif
