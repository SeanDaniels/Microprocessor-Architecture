LW R7 0(R1)
;set system middle loop counter
LW R8 0(R1)
;set system inner loop counter
LW R9 0(R1)
;set starting pointer for a[0][0]
LW R30 0(R2)
;set starting pointer for b[0][0]
LW R31 0(R3)
;set starting pointer for c[0][0]
LW R32 0(R4)
;clear temp index holder
XOR R5 R5 R5
;clear temp index holder
XOR R6 R6 R6
;clear colOffset reg
XOR R11 R11 R11
;;;;;;;;;;;;;;;;;;;;;;;;;
;; init rowOffset reg  ;;
;;;;;;;;;;;;;;;;;;;;;;;;;
;rowOffset needs to be number of columns * 4 (size of element)
;clear R11 reg
XOR R12 R11 R11
;add 4 to R12 reg
ADDI R12 R12 4
;multiply R12 by number of elements per row (number of columns(N))
MULT R12 R12 R7; R7==R8==R9==4
OUTERLOOP:  ADDI R5 R31 0
    ADDI R6 R31 0
;(R8 = numberOfColumns or numberOfRows)
    ADD R8 R8 R1
;(R9 = numberOfColumns or numberOfRows)
MIDDLELOOP: ADDI R9 R9 R1; reset inner loop counter
    XOR R25 R25 R25; clear sum register
INNERLOOP:  LW R40 0(R5)
;c[i][j] += a[i][k]*b[k][j];
;load a[i][k]
;load b[k][j]
    LW R41 0(R6)
;multiply a[i][k]*b[k][j]
    MULT R26 R40 R41
;someReg += a[i][k]*b[k][j]
    ADD R25 R25 R26
;increment a pointer
    ADDI R5 R5 4 ;increment a index by one col
;increment b pointer
    ADD R6 R6 R12 ;increment b index by one row
;decrement innerloop counter
    SUBI R9 R9 1
;(if R9 > 0)
    BNEZ R9 INNERLOOP
;else, store sumregValue (c[i][j])
    SW R25 0(R32)
;decrement middle loop counter
    SUBI R8 R8 1; decrement middleloop counter
;increment c index by one element
    ADD R32 R32 R11
;if(R8 > 0)
    BNEZ R8 MIDDLELOOP
;increment a index
    ADD R30 R30 R12
;increment c index
    ADD R32 R32 R12
;decrment outer loop counter
    SUBI R7 R7 1
;(if R7>0)
    BNEZ R7 OUTERLOOP
   EOP
