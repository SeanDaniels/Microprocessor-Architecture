
# Table of Contents

1.  [General Notes](#orgfc56526)
2.  [General Pipeline](#org35cc57b)
    1.  [IF/ID](#org1bb3101)
        1.  [Some SP Register](#orgcde1f08)
        2.  [Some other SP Register](#org594db0e)
3.  [Program Objects](#orgeabdf08)
    1.  [mips](#org3b0d1c7)
    2.  [sim<sub>t</sub>](#org99cf45b)
    3.  [instruction<sub>t</sub>](#orgc684ae9)
4.  [no<sub>dep</sub> asm](#orgae6f5c5)
5.  [Highlevel (from lecture)](#org0778b2d)
    1.  [Simulate the life of each instruction clock cycle by clock cycle](#org18ad160)
    2.  [Assembly file loads into simulator](#org5fc68d0)
        1.  [What to do with the assembly file](#orgda0f2db)
    3.  [Simulator outputs:](#orgf96f71e)
    4.  [Stages](#orgc315a3f)
        1.  [IF](#orgb4df78c)
        2.  [ID](#org71643cb)
        3.  [EXE](#orgc8315e9)
        4.  [MEM](#orgf1cd95a)
        5.  [WB](#orgefbfc79)
    5.  [What do I need to model?](#orgf88f085)
        1.  [Register File](#org80e967f)
        2.  [Pipeline Registers](#org57b42f0)
        3.  [Logic for implementing ALU](#org86e0a9c)
    6.  [What is the flow of the program](#org86685c1)
    7.  [What do I do in run()](#orgc73b0e3)
    8.  [Functions to init, terminate, and clear simulator](#org3fd701c)
    9.  [Function that models the cycle by cycle execution of the pipeline](#org30e958e)
    10. [Functions to init the registers and memory](#orge78f51e)
    11. [Functions to inspect register/memory](#org0076f87)
    12. [Parser](#orgabeb868)



<a id="orgfc56526"></a>

# General Notes

-   Run debugger, stop after load<sub>program</sub> function, inspect &rsquo;mips&rsquo; object
-   I think that the relevant information exists in the mips object (register
    values, instructions, destinations..)


<a id="org35cc57b"></a>

# General Pipeline


<a id="org1bb3101"></a>

## IF/ID


<a id="orgcde1f08"></a>

### Some SP Register

-   PC + 4 OR some other instruction address (multiplexer output, not sure what the control line)


<a id="org594db0e"></a>

### Some other SP Register

-   Instruction memory
    -   Input is instruction address
    -   Output is instruction


<a id="orgeabdf08"></a>

# Program Objects


<a id="org3b0d1c7"></a>

## mips

-   mips is an of type sim<sub>t</sub>


<a id="org99cf45b"></a>

## sim<sub>t</sub>

-   sim<sub>t</sub> is a struct with the following fields
    -   instruction<sub>t</sub> instr<sub>memory</sub>[PROGRAM<sub>SIZE</sub>]
    -   unsigned instr<sub>base</sub><sub>address</sub>
    -   unsigned char \*data<sub>memory</sub>
    -   unsigned data<sub>memory</sub><sub>size</sub>
    -   unsigned data<sub>memory</sub><sub>latency</sub>


<a id="orgc684ae9"></a>

## instruction<sub>t</sub>

-   instruction<sub>t</sub> is a struct with the following fields
    -   opcode<sub>t</sub> opcode
    -


<a id="orgae6f5c5"></a>

# no<sub>dep</sub> asm

LW	R1 0(R0)
ADD	R2 R3 R4
ADDI	R3 R3 10
SUB	R4 R4 R1
SW  	R2 0(R0)
ADD	R1 R2 R3
SW	R4 4(R0)
ADD	R5 R5 R6
LW	R6 4(R0)
EOP


<a id="org0778b2d"></a>

# Highlevel (from lecture)


<a id="org18ad160"></a>

## Simulate the life of each instruction clock cycle by clock cycle


<a id="org5fc68d0"></a>

## Assembly file loads into simulator


<a id="orgda0f2db"></a>

### What to do with the assembly file

-   At the end of clock cycle 0:
    -   Instruction 1 should be in ID stage
    -   Instruction 2 should be in IF stage
-   At the end of clock cycle 1
    -   Instruction 1 should be in EXE stage
    -   Instruction 2 should be in ID stage
    -   Instruction 3 should be in IF stage
-   


<a id="orgf96f71e"></a>

## Simulator outputs:

-   Values in register file
-   Values in the pipeline registers
-   Content of memory
-   CPI


<a id="orgc315a3f"></a>

## Stages


<a id="orgb4df78c"></a>

### IF

-   IF/ID


<a id="org71643cb"></a>

### ID

-   ID/EXE


<a id="orgc8315e9"></a>

### EXE

-   EXE/MEM


<a id="orgf1cd95a"></a>

### MEM

-   MEM/WB


<a id="orgefbfc79"></a>

### WB


<a id="orgf88f085"></a>

## What do I need to model?

32-bit register can be modeled through a 32 bit data type (unsigned data type)


<a id="org80e967f"></a>

### Register File


<a id="org57b42f0"></a>

### Pipeline Registers


<a id="org86e0a9c"></a>

### Logic for implementing ALU


<a id="org86685c1"></a>

## What is the flow of the program

-   Add to the code templates the date structures required to implement
    -   Pipeline register
    -   Register file
    -   Counters for the number of instructions executed, number of clock cycles,
        number of stalls
-   Write the code that models pipeline execution assuming no hazards
    -   Data memory will provide data within the clock cylce (no structural hazards)
    -   Code does not contain branches (no control hazards)
    -   Code does not contain flow dependencies
-   Progressively add handling of hazards


<a id="orgc73b0e3"></a>

## What do I do in run()

-   Inspect:
    -   Register file
    -   Memory
    -   Pipeline registers
-   Value of:
    -   Number of instructions
    -   Number of clock cycles run
    -   Number of stalls


<a id="org3fd701c"></a>

## Functions to init, terminate, and clear simulator

-   sim<sub>pip</sub>/~sim<sub>pip</sub>
-   reset


<a id="org30e958e"></a>

## Function that models the cycle by cycle execution of the pipeline

-   run


<a id="orge78f51e"></a>

## Functions to init the registers and memory

-   set<sub>gp</sub><sub>register</sub>
-   write<sub>memory</sub>


<a id="org0076f87"></a>

## Functions to inspect register/memory

-   get<sub>gp</sub><sub>register</sub>
-   get<sub>IPC</sub>
-   get<sub>instructions</sub><sub>executed</sub>
-   get<sub>clock</sub><sub>cycles</sub>
-   get<sub>stalls</sub>
-   print<sub>registers</sub>()


<a id="orgabeb868"></a>

## Parser

+load<sub>program</sub>

