#include "definitions.h"

#ifndef EXEC_UNIT_H
#define EXEC_UNIT_H

class exec_unit{

pipe_stage_three UNIT_REG;
unsigned latency;
unsigned num_cycles;

public:
  exec_unit(unsigned latency, pipe_stage_three);
  void run_cycle();
  bool done();
  void print_exec_unit();
  void reset();
  pipe_stage_three getRegister();
  unsigned get_remaining_cycles();
};
#endif
