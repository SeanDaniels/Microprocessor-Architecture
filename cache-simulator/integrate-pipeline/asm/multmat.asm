    ;; In a single loop with a 1-dimensional array, the pointer to the array entry is incremented every iteration
    ;; In a nested loop with a 2-dimensional array(row major), the pointer to the col entry is incremented on every iteration of the inner loop
    ;; the pointer to the row entry is incremented when the inner loop finishes
OUTERLOOP:	LW R5 0(R2)             ;load value from array 1
	LW R6 0(R3)                 ;load value from array 2
	ADD R5 R5 R6                ;perform addition
	SW R5 0(R4)                 ;store result
	ADDI R2 R2 4                ;increment pointer for array 1
	ADDI R3 R3 4                ;increment pointer for array 2
	ADDI R4 R4 4                ;increment pointer for array 3
	SUBI R1 R1 1                ;decrement counter, for this code r1 is the counter, and its init'd to the number of elements in each array
	BNEZ R1 OUTERLOOP                ;go back through loop
EOP	
