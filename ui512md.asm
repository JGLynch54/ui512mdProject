;
;			ui512md
;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;
;			File:			ui512md.asm
;			Author:			John G. Lynch
;			Legal:			Copyright @2024, per MIT License below
;			Date:			June 20, 2024
;
;			Notes:
;				ui512 is a small project to provide basic operations for a variable type of unsigned 512 bit integer.
;
;				ui512a provides basic operations: zero, copy, compare, add, subtract.
;				ui512b provides basic bit-oriented operations: shift left, shift right, and, or, not, least significant bit and most significant bit.
;               ui512md provides multiply and divide.
;
;				It is written in assembly language, using the MASM (ml64) assembler provided as an option within Visual Studio.
;				(currently using VS Community 2022 17.9.6)
;
;				It provides external signatures that allow linkage to C and C++ programs,
;				where a shell/wrapper could encapsulate the methods as part of an object.
;
;				It has assembly time options directing the use of Intel processor extensions: AVX4, AVX2, SIMD, or none:
;				(Z (512), Y (256), or X (128) registers, or regular Q (64bit)).
;
;				If processor extensions are used, the caller must align the variables declared and passed
;				on the appropriate byte boundary (e.g. alignas 64 for 512)
;
;				This module is very light-weight (less than 1K bytes) and relatively fast,
;				but is not intended for all processor types or all environments. 
;
;				Use for private (hobbyist), or instructional, or as an example for more ambitious projects is all it is meant to be.
;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;
;			MIT License
;
;			Copyright (c) 2024 John G. Lynch
;
;				Permission is hereby granted, free of charge, to any person obtaining a copy
;				of this software and associated documentation files (the "Software"), to deal
;				in the Software without restriction, including without limitation the rights
;				to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;				copies of the Software, and to permit persons to whom the Software is
;				furnished to do so, subject to the following conditions:
;
;				The above copyright notice and this permission notice shall be included in all
;				copies or substantial portions of the Software.
;
;				THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;				IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;				FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;				AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;				LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;				OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
;				SOFTWARE.
;
;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------

				INCLUDE			ui512aMacros.inc
				INCLUDE			ui512bMacros.inc
				INCLUDE			ui512mdMacros.inc

				OPTION			casemap:none
.CODE			ui512md
				OPTION			PROLOGUE:none
				OPTION			EPILOGUE:none

				MemConstants

;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		mult_u:PROC					; void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier)
;			mult_u			-	multiply 512 multiplicand by 512 multiplier, giving 512 product, overflow
;			Prototype:		-	void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier);
;			product			-	Address of 8 QWORDS to store resulting product (in RCX)
;			overflow		-	Address of 8 QWORDS to store resulting overflow (in RDX)
;			multiplicand	-	Address of 8 QWORDS multiplicand (in R8)
;			multiplier		-	Address of 8 QWORDS multiplier (in R9)
;			returns			-	nothing (0)
;
mult_u			PROC			PUBLIC
				LOCAL			padding1 [ 8 ] : QWORD
				LOCAL			product [ 16 ] : QWORD
				LOCAL			savedRBP : QWORD
				LOCAL			savedRCX : QWORD, savedRDX : QWORD, savedR10 : QWORD, savedR11 : QWORD, savedR12 : QWORD
				LOCAL			plierl : WORD						; low limit index of of multiplier (7 - first non-zero)
				LOCAL			candl : WORD						; low limit index of multiplicand
				LOCAL			padding2 [ 16 ] : QWORD
mult_u_ofs		EQU				padding2 + 64 - padding1			; offset is the size of the local memmory declarations

				CreateFrame		220h, savedRBP
				MOV				savedRCX, RCX
				MOV				savedRDX, RDX
				MOV				savedR10, R10
				MOV				savedR11, R11
				MOV				savedR12, R12

				CheckAlign		RCX									; (out) Product
				CheckAlign		RDX									; (out) Overflow
				CheckAlign		R8									; (in) Multiplicand
				CheckAlign		R9									; (in) Multiplier

; Examine multiplicand, save dimensions, handle edge cases of zero or one
				MOV				RCX, R8								; examine multiplicand
				CALL			msb_u
				CMP				AX, -1								; multiplicand = 0? exit with product = 0
				JE				@@zeroandexit
				CMP				AX, 0								; multiplicand = 1?	exit with product = multiplier
				MOV				RDX, R9								; address of multiplier (to be copied to product)
				JE				@@copyandexit
				SHR				AX, 6								; divide by 64 to get Nr words
				MOV				CX, 7 
				SUB				CX, AX								; subtract from 7 to get starting (high order) begining index
				MOV				candl, CX							; save off multiplicand index lower limit (eliminate multiplying leading zero words)

; Examine multiplier, save dimensions, handle edge cases of zero or one
				MOV				RCX, R9								; examine multiplier
				CALL			msb_u
				CMP				AX, -1								; multiplier = 0? exit with product = 0
				JE				@@zeroandexit
				CMP				AX, 0								; multiplier = 1? exit with product = multiplicand
				MOV				RDX, R8								; address of multiplicand (to be copied to product)
				JE				@@copyandexit
				SHR				AX, 6
				MOV				RCX, 7
				SUB				CX, AX
				MOV				plierl, CX							; save off multiplier index lower limit (eliminate multiplying leading zero words)

; In heap / frame / stack reserved memory, clear 16 qword area for overflow/product; set up indexes for loop
				LEA				RCX, product [ 0 ]
				Zero512			RCX									; clear working copy of overflow, need to start as zero, results are added in
				LEA				RCX, product [ 8 * 8 ]
				Zero512			RCX									; clear working copy of product (they need to be contiguous, so using working copy, not callers)
				MOV				R11, 7								; index for multiplier (reduced until less then saved plierl)
				MOV				R12, R11							; index for multiplicand (reduced until less than saved candl)

; multiply loop: an outer loop for each non-zero qword of multiplicand, with an inner loop for each non-zero qword of multiplier
@@multloop:
				MOV				R10, R11							; R10 holds index for overflow / product work area (results)
				ADD				R10, R12
				INC				R10									; index for product/overflow 
				MOV				RAX, [ R8 ] + [ R12 * 8 ]			; get qword of multiplicand
				MUL				Q_PTR [ R9 ] + [ R11 * 8 ]			; multiply by qword of multiplier
				ADD				product [ R10 * 8 ], RAX
				DEC				R10									; preserves carry flag
@@:
				ADC				product [ R10 * 8 ], RDX
				MOV				RDX, 0								; again, preserves carry flag
				JNC				@F
				DEC				R10
				JGE				@B
@@:																	; next qword of multiplicand
				DEC				R12
				CMP				R12W, candl
				JGE				@@multloop
				MOV				R12, 7
				DEC				R11
				CMP				R11W, plierl
				JGE				@@multloop							; next qword of multiplier

; copy working product/overflow to callers product / overflow
				MOV				RCX, savedRCX
				LEA				RDX, product [ 8 * 8 ]
				Copy512			RCX, RDX							; copy working product to callers product
				MOV				RCX, savedRDX
				LEA				RDX, product [ 0 ]
				Copy512			RCX, RDX							; copy working overflow to callers overflow

; restore regs, release frame, return
@@exit:			
				MOV				R10, savedR10
				MOV				R11, savedR11
				MOV				R12, savedR12						; restore any non-volitile regs used
				ReleaseFrame	savedRBP
				XOR				RAX, RAX							; return zero
				RET

; zero callers product and overflow
@@zeroandexit:
				MOV				RCX, savedRCX						; reload address of callers product
				Zero512			RCX									; zero it
				MOV				RCX, savedRDX						; reload address of caller overflow
				Zero512			RCX									; zero it
				JMP				@@exit

; multiplying by 1: zero overflow, copy the non-one to the product
@@copyandexit:
				MOV				RCX, savedRDX						; address of passed overflow
				Zero512			RCX 								; zero it
				MOV				RCX, savedRCX						; copy (whichever: multiplier or multiplicand) to callers product
				Copy512			RCX, RDX							; RDX "passed" here from whomever jumped here (either multiplier, or multiplicand in RDX)
				JMP				@@exit								; and exit
mult_u			ENDP

;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		mult_uT64:PROC				;	void mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);
;			mult_uT64		-	multiply 512 bit multiplicand by 64 bit multiplier, giving 512 product, 64 bit overflow
;			Prototype:		-	void mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);
;			product			-	Address of 8 QWORDS to store resulting product (in RCX)
;			overflow		-	Address of QWORD for resulting overflow (in RDX)
;			multiplicand	-	Address of 8 QWORDS multiplicand (in R8)
;			multiplier		-	multiplier QWORD (in R9)
;			returns			-	nothing (0)

				OPTION			PROLOGUE:none
				OPTION			EPILOGUE:none
mult_uT64		PROC			PUBLIC
				
				CheckAlign		RCX
				CheckAlign		R8

; caller might be doing multiply 'in-place', so need to save the original multiplicand, prior to clearing callers product
				FOR				idx, < 0, 1, 2, 3, 4, 5, 6, 7 >
				PUSH			Q_PTR [ R8 ] [ idx * 8 ]
				ENDM

; clear callers product and overflow
;	Note: if caller used multiplicand and product as the same variable (memory space),
;	this would wipe the multiplicand. Hence the saving of the multiplicand on the stack. (above)
				XOR				RAX, RAX
				Zero512			RCX									; clear callers product (multiply uses an additive carry, so it needs to be zeroed)
				MOV				[ RDX ], RAX						; clear callers overflow
 				MOV				R10, RDX							; RDX (pointer to callers overflow) gets used in the MUL: save it in R10

; FOR EACH index of 7 thru 1 (omiting 0): fetch qword of multiplicand, multiply, add 128 bit (RAX, RDX) to running working
				FOR				idx, <7, 6, 5, 4, 3, 2, 1>
				POP				RAX									; multiplicand [ idx ] qword -> RAX
				MUL				R9									; times multiplier -> RAX, RDX
				ADD				[ RCX ] [ idx * 8 ], RAX			; add RAX to working product [ idx ] qword
				ADC				[ RCX ] [ (idx - 1) * 8 ], RDX		; and add with carry to [ idx - 1 ] qword of working product
				ENDM

; Most significant (idx=0), the high order result of the multiply in RDX, goes to the overflow of the caller
				POP				RAX
				MUL				R9
				ADD				[ RCX ] [ 0 * 8 ], RAX
				ADC				[ R10 ], RDX						; last qword overflow is also the operation overflow
				XOR				RAX, RAX							; return zero
				RET
mult_uT64		ENDP

;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		div_u:PROC					; s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor)
;			div_u			-	divide 512 bit dividend by 512 bit divisor, giving 512 bit quotient and remainder
;			Prototype:		-	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor);
;			quotient		-	Address of 8 QWORDS to store resulting quotient (in RCX)
;			remainder		-	Address of 8 QWORDs for resulting remainder (in RDX)
;			dividend		-	Address of 8 QWORDS dividend (in R8)
;			divisor			-	Address of 8 QWORDs divisor (in R9)
;			returns			-	0 for success, -1 for attempt to divide by zero

				OPTION			PROLOGUE:none
				OPTION			EPILOGUE:none
div_u			PROC			PUBLIC

				LOCAL			padding1 [ 16 ] : QWORD
				LOCAL			currnumerator [ 16 ] : QWORD
				LOCAL			qdiv [ 16 ] : QWORD, quotient [ 8 ] : QWORD, normdivisor [ 8 ] : QWORD
				LOCAL			savedRCX : QWORD, savedRDX : QWORD, savedR8 : QWORD, savedR9 : QWORD
				LOCAL			savedR10 : QWORD, savedR11 : QWORD, savedR12 : QWORD
				LOCAL			savedRBP : QWORD
				LOCAL			qHat : QWORD, rHat : QWORD,	nDiv : QWORD, addbackRDX : QWORD, addbackR11 : QWORD
				LOCAL			normf : WORD, jIdx : WORD, mIdx : WORD, nIdx : WORD, mDim : WORD, nDim : Word			
				LOCAL			padding2 [ 16 ] : QWORD
div_oset		EQU				padding2 + 64 - padding1

				CreateFrame		360h, savedRBP
				MOV				savedRCX, RCX
				MOV				savedRDX, RDX
				MOV				savedR8, R8
				MOV				savedR9, R9
				MOV				savedR10, R10
				MOV				savedR11, R11
				MOV				savedR12, R12

				CheckAlign		RCX									; (out) Quotient
				CheckAlign		RDX									; (out) Remainder
				CheckAlign		R8									; (in) Dividend
				CheckAlign		R9									; (in) Divisor

; Initialize
				Zero512			RCX									; zero callers quotient
				MOV				RCX, RDX
				Zero512			RCX									; zero callers remainder
				LEA				RCX, quotient
				Zero512			RCX									; zero working quotient

 ; Examine divisor
				MOV				RCX, R9								; divisor
				CALL			msb_u								; most significant bit
				CMP				AX, -1								; msb < 0? 
				JE				divbyzero							; divisor is zero, abort
				CMP				AX, 64								; divisor only one 64-bit word?
				JMP				mbynDiv								; no, do divide of m digit by n digit				*** NOTE: restore JGE after testing (allows full div on single word)

;	divide of m 64-bit qwords by one 64 bit qword divisor, use the quicker divide routine (div_uT64), and return
				MOV				RCX, savedRCX						; set up parms for call to div by 64bit: RCX - addr of quotient
				MOV				RDX, savedRDX						; RDX - addr of remainder
				Zero512			RDX									
				MOV				R8, savedR8							; R8 - addr of dividend
				MOV				RAX, savedR9
				MOV				R9, Q_PTR [RAX + 7 * 8 ]			; R9 - value of 64 bit divisor
				CALL			div_uT64
				MOV				RDX, savedRDX						; move 64 bit remainder to last word of 8 word remainder
				MOV				RCX, Q_PTR [ RDX ]					; get the one qword remainder
				Zero512			RDX									; clear the 8 qword callers remainder
				MOV				[ RDX ] [ 7 * 8 ], RCX				; put the one qword remainder in the least significant qword of the callers remainder
				JMP				cleanupret

;
; Going to divide an 'm' digit dividend (u), by an 'n' digit divisor (v)
;	See Knuth, The Art of Computer Programming, Volume 2, Algorithm D, Pages 272-278
;	Notes: This is much like the long division done by hand as taught in school, but instead of digits being 0 to 9, they are 0 to 2^64, or whole 64 bit qwords.
;	Knuth suggests any base can work, but suggests the selection of one more 'natural' to the machine. Since x64 machines have a 128-bit by 64 bit divide instruction,
;	The selected base 'b' is one whole qword, or 64 bits, or 2^64. As in manual division, the first non-obvious step is to 'align' the leading bits of the divisor. Knuth calls this
;	'normalization'. It requires determining the dimensions of the variables, and the most significant bit of the divisor. Both variables are then shifted (left) to get the 
;	most significant bit of the divisor into the most significant bit of the qword in which it is found. This makes the leading digit of the divisor greter than or equal to 2^64 / 2,
;	making the first division meaningful. The dividend must then be shifted the same amount.
;	When the division is completed, no action needs to be taken on the quotient - it will be correct without shifting back (the shifts of dividend and divisor cancel each other out).
;	The remainder will need to be shifted (right) to "de-normalize" for the return value.
;	This process yields the dimensions of the variables: the number of qwords in each: 'm' for dividend, 'n' for divisor. Midx, and Nidx are the starting indexes, reflecting m and n.
;
mbynDiv:
				MOV				nDim, AX							; still have divisor msb in AX
				SHR				nDim, 6								; div msb by 64 to get msq (most significant qword) aka 'n' (zero based Nr qwords)
				MOV				CX, 7
				SUB				CX, nDim
				INC				nDim
				MOV				nIdx, CX							; nIdx now 7 - msb of leading word: aka idx to first qword
				;
				MOV				normf, AX
				AND				normf, 63
				MOV				AX, 63
				SUB				AX, normf							; Nr bits to get leading divisor bit to msb saved at normf
				MOV				normf, AX
																	
;	Notes: (continued from above) The mIdx and nIdx are indexes pointing the the leading non-zero qword in dividend and divisor respectively.
;	jIDx is the number of non-zero qwords in the dividend (after normalization shift). It starts at first non-zero qword

; Step D1: Normalize	
				
				MOV				RDX, savedR9						; callers divisor
				LEA				RCX, normdivisor					; local copy of divisor, normalized
				MOV				R8W, normf
				CALL			shl_u								; by shifting until MSB is high bit								
				;
				MOV				RDX, savedR8						; callers dividend
				LEA				RCX, currnumerator [ 8 * 8 ]		; starting numerator is the normalized supplied dividend
				MOV				R8W, normf							; shifted the same Nr bits as it took to make divisor msb the leading bit
				CALL			shl_u
				LEA				RDX, currnumerator					; zero first eight words of working enumerator
				Zero512			RDX
				MOV				RDX, savedR8
				MOV				RAX, [ RDX ]						; if numerator has 8 qwords (dimM = 7), then the shift just lost us high order bits, get them			
				MOV				CX, 63								; shift first qword of dividend right 
				SUB				CX, normf							; by 63 minus Nr bits shifted left
				SHR				RAX, CL								; eliminating all bit the bits we lost
				LEA				R12, currnumerator [ 7 * 8 ]
				MOV				[ R12 ], RAX						; store them at m + 1 (the word 'before' the other up to eight qwords)
				;
				XOR				RAX, RAX
				MOV				RCX, -1
@@:
				TEST			[ R12 ] + [ RAX * 8 ], RCX			; R12 currently has addr of begining of enumerator, 
				JNZ				@F
				INC				RAX
				CMP				RAX, 8
				JLE				@B
@@:
				LEA				R12, [ R12 ] + [ RAX * 8 ]			; revise begining address of working numerator with address of first non-zero qword
				LEA				RCX, [ 9 ]
				SUB				CX, AX
				MOV				mDim, CX
				MOV				mIdx, AX							; mIdx after normalize
				MOV				jIdx, AX							; loop counter (Nr words in denominator)
				CMP				CX, nDim 
				JL				numtoremain							; if dimension M (dividend) is less than dimension N (divisor), exit with result zero
				MOVZX			RAX, nIdx							; first word of divisor
				MOV				RAX, normdivisor [ RAX * 8 ]
				MOV				nDiv, RAX							; save and re-use first qword of divisor (used each time to determine qhat)
				LEA				R12, currnumerator [ 7 * 8 ]

;			Step D3: Calculate  q^
D3:
				MOVZX			RCX, jIdx
				MOV				RDX, [ R12 ] + [ RCX * 8 ]
				INC				RCX
				MOV				RAX, [ R12 ] + [ RCX * 8 ]
				DIV				nDiv
				MOV				qHat, RAX
				MOV				rHat, RDX

;			Step D4: Multiply trial quotient digit by full normalized divisor, then subtract from working copy of numerator (tricky alignment issues)
D4:
				LEA				RCX, qdiv
				Zero512			RCX
				LEA				RCX, qdiv [ 8 * 8 ]					; qdiv is 16 qwords. Last eight are answer
				LEA				RDX, qdiv [ 7 * 8 ]					; Nr 7 will be overflow (if any)
				LEA				R8, normdivisor						; the normalized 8 qword divisor
				MOV				R9, qHat							; times qhat
				CALL			mult_uT64							; multiply divisor by trial quotient digit (qHat)

		; a little intricate here: 	Need the starting address of the two operands, the shorter of the two (remaining) lengths
		; expaining the intricacies: the leading digit of the multiplied number needs to line up with the leading digit of the enumerator @ [ R12 ]
		; the multiply may have added a digit (a qword). The added digit may be in the answer, or may be in the overflow
		; subtract the result of the multiply from the remaining (current) numerator

;			Step D5: Test remainder
D5:
				MOVZX			RAX, jIdx
				MOV				RCX, qHat
				MOV				quotient [ RAX * 8 ], RCX			; Set quotient digit [ j ] 
				JNC				D7									; Carry (from above) indicates result of subtract went negative, need D6 add back, else on to D7

;			Step D6: Add Back
D6:
				DEC				quotient [ RAX * 8 ]				; adjust quotient digit
				MOV				RDX, addbackRDX						; restore same indexes / counters used in subtract for add back
				MOV				R10, addbackR11
				CLC													; Carry indicator
@@:
				MOV				RAX, [ R11 ] + [ RDX * 8 ]
				LEA				RCX, [ R12 ] + [ 1 * 8 ]
				ADC				[ RCX ] + [ RDX * 8 ], RAX
				DEC				RDX
				DEC				R10
				JNS				@B
				MOV				RAX, qdiv [ 7 * 8 ]
				ADC				[ R12 ] + [ RDX * 8 ], RAX

;			Step D7: Loop on j
D7:
				INC				jIdx								; increase loop counter
				CMP				jIdx, 8								; done?
				JLE				D3									; no, loop to D3

;			Step D8: Un Normalize:
D8UnNormalize:
				MOV				RCX, savedRDX						; reduced working numerator is now the remainder
				LEA				RDX, currnumerator + [ 8 * 8 ]		; shifted result to callers remainder
				MOV				R8W, normf
				CALL			shr_u
				;
				MOV				RCX, savedRCX						; copy working quotient to callers quotient
				LEA				RDX, quotient
				Copy512			RCX, RDX
cleanupret:
				XOR				RAX, RAX							; return zero
cleanupwretcode:
				MOV				R12, savedR12
				MOV				R11, savedR11
				MOV				R10, savedR10
				MOV				R9,  savedR9
				MOV				R8,  savedR8
				MOV				RDX, savedRDX
				MOV				RCX, savedRCX						; restore parameter registers back to "as-called" values
				ReleaseFrame	savedRBP
				RET
divbyzero:
				MOV				EAX, ret_1
				JMP				cleanupwretcode

numtoremain:	MOV				R8, savedR8
				MOV				RDX, savedRDX
				Copy512			RDX, R8
				JMP				cleanupret

div_u			ENDP

;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		div_uT64:PROC				; s16 div_uT64( u64* quotient, u64* remainder, u64* dividend, u64 divisor)
;			div_uT64		-	divide 512 bit dividend by 64 bit divisor, giving 512 bit quotient and 64 bit remainder
;			Prototype:		-	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64 divisor);
;			quotient		-	Address of 8 QWORDS to store resulting quotient (in RCX)
;			remainder		-	Address of QWORD for resulting remainder (in RDX)
;			dividend		-	Address of 8 QWORDS dividend (in R8)
;			divisor			-	Value of 64 bit divisor (in R9)
;			returns			-	0 for success, -1 for attempt to divide by zero
;
;			Regs with contents destroyed, not restored: RAX, RDX, R10 (each considered volitile, but caller might optimize on other regs)

				OPTION			PROLOGUE:none
				OPTION			EPILOGUE:none
div_uT64		PROC			PUBLIC

				CheckAlign		RCX
				CheckAlign		R8

; Test divisor for divide by zero				
				TEST			R9, R9
				JZ				@@DivByZero

; DIV instruction (64-bit) uses RAX and RDX. Need to move RDX (addr of remainder) out of the way; start it off with zero
				MOV				R10, RDX
				XOR				RDX, RDX

; FOR EACH index of 0 thru 7: get qword of dividend, divide by divisor, store qword of quotient
				FOR				idx, < 0, 1, 2, 3, 4, 5, 6, 7 >
				MOV				RAX, Q_PTR [ R8 ] [ idx * 8 ]		; dividend [ idx ] -> RAX
				DIV				R9									; divide by divisor in R9
				MOV				[ RCX ] [ idx * 8 ], RAX			; quotient [ idx ] <- RAX ; Note: remainder in RDX for next divide
				ENDM

; Last (least significant qword) divide leaves a remainder, store it at callers remainder
				MOV				[ R10 ], RDX						; remainder to callers remainder
				XOR				RAX, RAX							; return zero
@@exit:			
				RET

; Exception handling, divide by zero
@@DivByZero:
				Zero512			RCX									; Divide by Zero. Could throw fault, but returning zero quotient, zero remainder
				XOR				RAX, RAX
				MOV				[ R10 ] , RAX
				DEC				EAX									; return error (div by zero)
				JMP				@@exit
div_uT64		ENDP

				END