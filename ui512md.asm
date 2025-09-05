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

				INCLUDE			legalnotes.inc
				INCLUDE			compile_time_options.inc
				INCLUDE			ui512aMacros.inc
				INCLUDE			ui512bMacros.inc
				INCLUDE			ui512mdMacros.inc

				OPTION			CASEMAP:NONE
				OPTION			PROLOGUE:NONE
				OPTION			EPILOGUE:NONE

ui512D			SEGMENT			"CONST" ALIGN (64)					; Declare a data segment. Read only. Aligned 64.

				MemConstants

; end of memory resident constants
ui512D			ENDS												; end of data segment

;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		mult_u:PROC					; s16 mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier)
;			mult_u			-	multiply 512 multiplicand by 512 multiplier, giving 512 product, 512 overflow
;			Prototype:		-	s16 mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier);
;			product			-	Address of 8 QWORDS to store resulting product (in RCX)
;			overflow		-	Address of 8 QWORDS to store resulting overflow (in RDX)
;			multiplicand	-	Address of 8 QWORDS multiplicand (in R8)
;			multiplier		-	Address of 8 QWORDS multiplier (in R9)
;			returns			-	(0) for success, (GP_Fault) for mis-aligned parameter address
;
		
				Other_Entry		mult_u, ui512
mult_u			PROC			PUBLIC
				LOCAL			padding1 [ 8 ] : QWORD
				LOCAL			product [ 16 ] : QWORD
				LOCAL			savedRBP : QWORD
				LOCAL			savedRCX : QWORD, savedRDX : QWORD, savedR10 : QWORD, savedR11 : QWORD, savedR12 : QWORD
				LOCAL			plierl : WORD						; low limit index of of multiplier (7 - first non-zero)
				LOCAL			candl : WORD						; low limit index of multiplicand
				LOCAL			padding2 [ 16 ] : QWORD
mult_u_ofs		EQU				padding2 + 64 - padding1			; offset is the size of the local memory declarations

				CreateFrame		220h, savedRBP
				MOV				savedRCX, RCX
				MOV				savedRDX, RDX
				MOV				savedR10, R10
				MOV				savedR11, R11
				MOV				savedR12, R12

; Check passed parameters alignment, since this is checked within frame, need to specify exit / cleanup / unwrap label
				CheckAlign		RCX, @@exit							; (out) Product
				CheckAlign		RDX, @@exit							; (out) Overflow
				CheckAlign		R8, @@exit							; (in) Multiplicand
				CheckAlign		R9, @@exit							; (in) Multiplier

; Examine multiplicand, save dimensions, handle edge cases of zero or one
				MOV				RCX, R8								; examine multiplicand
				CALL			msb_u								; get count to most significant bit (-1 if no bits)
				CMP				AX, 0								
				JL				@@zeroandexit						; msb < 0? multiplicand = 0; exit with product = 0
				LEA				RDX, [ R9 ]							; multiplicand = 1?	exit with product = multiplier -> address of multiplier (to be copied to product)
				JE				@@copyandexit
				SHR				AX, 6								; divide msb by 64 to get Nr words
				LEA				CX, [ 7 ]
				SUB				CX, AX								; subtract from 7 to get starting (high order, left-most) beginning index
				MOV				candl, CX							; save off multiplicand index lower limit (eliminate multiplying leading zero words)

; Examine multiplier, save dimensions, handle edge cases of zero or one
				MOV				RCX, R9								; examine multiplier
				CALL			msb_u								; get count to most significant bit (-1 if no bits)
				CMP				AX, 0								; multiplier = 0? exit with product = 0
				JL				@@zeroandexit
				LEA				RDX, [ R8 ]							; multiplier = 1? exit with product = multiplicand -> address of multiplicand (to be copied to product)
				JE				@@copyandexit
				SHR				AX, 6								; divide msb by 64 to get Nr words
				LEA				CX, [ 7 ]
				SUB				CX, AX								; subtract from 7 to get starting (high order, left-most) beginning index
				MOV				plierl, CX							; save off multiplier index lower limit (eliminate multiplying leading zero words)

; In frame / stack reserved memory, clear 16 qword area for working version of overflow/product; set up indexes for loop
				LEA				RCX, product [ 0 ]
				Zero512			RCX									; clear working copy of overflow, need to start as zero, results are accumulated
				LEA				RCX, product [ 8 * 8 ]
				Zero512			RCX									; clear working copy of product (they need to be contiguous, so using working copy, not callers)
				LEA				R11, [ 7 ] 							; index for multiplier (reduced until less than saved plierl) (outer loop)
				LEA				R12, [ R11 ]						; index for multiplicand (reduced until less than saved candl) (inner loop)

; multiply loop: an outer loop for each non-zero qword of multiplicand, with an inner loop for each non-zero qword of multiplier, results accumulated in 'product'
@@multloop:
				LEA				R10, [ R11 ] [ R12 ]				; R10 holds index for overflow / product work area (results)
				INC				R10									; index for product/overflow 
				MOV				RAX, Q_PTR [ R8 ] [ R12 * 8 ]		; get qword of multiplicand
				MUL				Q_PTR [ R9 ] [ R11 * 8 ]			; multiply by qword of multiplier
				ADD				product [ R10 * 8 ], RAX			; accummulate in product, this is low-order 64 bits of result of mul
				DEC				R10									; preserves carry flag
@@:
				ADC				product [ R10 * 8 ], RDX			; high-order result of 64bit multiply, plus the carry (if any)
				LEA				RDX, [ 0 ]							; again, preserves carry flag
				JNC				@F									; if adding caused carry, propagate it, else next 
				DEC				R10									; propagating carry
				JGE				@B
@@:																	; next qword of multiplicand
				DEC				R12
				CMP				R12W, candl							; Done with inner loop?
				JGE				@@multloop							; no, do it again
				LEA				R12, [ 7 ]							; yes, reset inner loop (multiplicand) index
				DEC				R11									; decrement index for outer loop
				CMP				R11W, plierl						; done with outer loop?
				JGE				@@multloop							; no, do it again with next qword of multiplier

; finished: copy working product/overflow to callers product / overflow
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
				Copy512			RCX, RDX							; RDX "passed" here from whomever jumped here (either &multiplier, or &multiplicand in RDX)
				JMP				@@exit								; and exit
				
mult_u			ENDP				
				Other_Exit		mult_u, ui512

;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		mult_uT64:PROC				;	s16 mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);
;			mult_uT64		-	multiply 512 bit multiplicand by 64 bit multiplier, giving 512 product, 64 bit overflow
;			Prototype:		-	s16 mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);
;			product			-	Address of 8 QWORDS to store resulting product (in RCX)
;			overflow		-	Address of QWORD for resulting overflow (in RDX)
;			multiplicand	-	Address of 8 QWORDS multiplicand (in R8)
;			multiplier		-	multiplier QWORD (in R9)
;			returns			-	(0) for success, (GP_Fault) for mis-aligned parameter address
				Other_Entry		mult_uT64, ui512
mult_uT64		PROC			PUBLIC

; Check passed parameters alignment, since this is checked within frame, need to specify exit / cleanup / unwrap label
				CheckAlign		RCX, @@exit							; (out) Product
				CheckAlign		R8, @@exit							; (in) Multiplicand

; caller might be doing multiply 'in-place', so need to save the original multiplicand, prior to clearing callers product (A = A * x), or (A *= x)
				FOR				idx, < 0, 1, 2, 3, 4, 5, 6, 7 >
				PUSH			Q_PTR [ R8 ] [ idx * 8 ]
				ENDM

; clear callers product and overflow
;	Note: if caller used multiplicand and product as the same variable (memory space),
;	this would wipe the multiplicand. Hence the saving of the multiplicand on the stack. (above)
				XOR				RAX, RAX
				Zero512Q		RCX		   							; clear callers product (multiply uses an addition with carry, so it needs to start zeroed)
				MOV				Q_PTR [ RDX ], RAX					; clear callers overflow
 				MOV				R10, RDX							; RDX (pointer to callers overflow) gets used in the MUL: save it in R10

; FOR EACH index of 7 thru 1 (omiting 0): fetch (pop) qword of multiplicand, multiply, add 128 bit result (RAX, RDX) to running working product
				FOR				idx, < 7, 6, 5, 4, 3, 2, 1 >		; Note: this is not a 'real' for, this is a macro that generates an unwound loop
				POP				RAX									; multiplicand [ idx ] qword -> RAX
				MUL				R9									; times multiplier -> RAX, RDX
				ADD				Q_PTR [ RCX ] [ idx * 8 ], RAX		; add RAX to working product [ idx ] qword
				ADC				Q_PTR [ RCX ] [ (idx - 1) * 8 ], RDX	; and add RDX with carry to [ idx - 1 ] qword of working product
				ENDM

; Most significant (idx=0), the high order result of the multiply in RDX, goes to the overflow of the caller
				POP				RAX
				MUL				R9
				ADD				Q_PTR [ RCX ] [ 0 * 8 ], RAX
				ADC				Q_PTR [ R10 ], RDX					; last qword overflow is also the operation overflow
				XOR				RAX, RAX							; return zero
@@exit:
				RET
				
mult_uT64		ENDP
				Other_Exit		mult_uT64, ui512

;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		div_u:PROC					; s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor)
;			div_u			-	divide 512 bit dividend by 512 bit divisor, giving 512 bit quotient and remainder
;			Prototype:		-	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor);
;			quotient		-	Address of 8 QWORDS to store resulting quotient (in RCX)
;			remainder		-	Address of 8 QWORDs for resulting remainder (in RDX)
;			dividend		-	Address of 8 QWORDS dividend (in R8)
;			divisor			-	Address of 8 QWORDs divisor (in R9)
;			returns			-	0 for success, -1 for attempt to divide by zero, (GP_Fault) for mis-aligned parameter address

				Other_Entry		div_u, ui512
div_u			PROC			PUBLIC
				LOCAL			padding1 [ 16 ] : QWORD
				LOCAL			currnumerator [ 16 ] : QWORD
				LOCAL			qdiv [ 16 ] : QWORD, quotient [ 8 ] : QWORD, normdivisor [ 8 ] : QWORD
				LOCAL			savedRCX : QWORD, savedRDX : QWORD, savedR8 : QWORD, savedR9 : QWORD
				LOCAL			savedR10 : QWORD, savedR11 : QWORD, savedR12 : QWORD, savedRBP : QWORD
				LOCAL			qHat : QWORD, rHat : QWORD,	nDiv : QWORD
				LOCAL			sublen: QWORD, addbackRDX : QWORD, addbackR11 : QWORD
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

				CheckAlign		RCX, cleanupwretcode				; (out) Quotient
				CheckAlign		RDX, cleanupwretcode				; (out) Remainder
				CheckAlign		R8, cleanupwretcode					; (in) Dividend
				CheckAlign		R9, cleanupwretcode					; (in) Divisor

; Initialize
				Zero512			RCX									; zero callers quotient
				Zero512			RDX									; zero callers remainder
				LEA				RCX, quotient
				Zero512			RCX									; zero working quotient

 ; Examine divisor
				MOV				RCX, R9								; divisor
				CALL			msb_u								; get most significant bit
				CMP				AX, 0								; msb < 0? 
				JL				divbyzero							; divisor is zero, abort
				JE				divbyone							; divisor is one, exit with remainder = 0, quotient = dividend 
				CMP				AX, 64								; divisor only one 64-bit word?
				JGE				mbynDiv								; no, do divide of m digit by n digit

;	divide of m 64-bit qwords by one 64 bit qword divisor, use the quicker divide routine (div_uT64), and return
				MOV				RCX, savedRCX						; set up parms for call to div by 64bit: RCX - addr of quotient
				MOV				RDX, savedRDX						; RDX - addr of remainder
				MOV				R8, savedR8							; R8 - addr of dividend
				MOV				RAX, savedR9
				MOV				R9, Q_PTR [ RAX ] [ 7 * 8 ]			; R9 - value of 64 bit divisor
				CALL			div_uT64
				MOV				RDX, savedRDX						; move 64 bit remainder to last word of 8 word remainder
				MOV				RCX, Q_PTR [ RDX ]					; get the one qword remainder
				Zero512			RDX									; clear the 8 qword callers remainder
				MOV				Q_PTR [ RDX ] [ 7 * 8 ], RCX		; put the one qword remainder in the least significant qword of the callers remainder
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
		;		INC				nDim
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
				MOV				RAX, Q_PTR [ RDX ]					; if numerator has 8 qwords (dimM = 7), then the shift just lost us high order bits, get them			
				MOV				CX, 63								; shift first qword of dividend right 
				SUB				CX, normf							; by 63 minus Nr bits shifted left
				SHR				RAX, CL								; eliminating all bit the bits we lost
				LEA				R12, currnumerator [ 7 * 8 ]
				MOV				[ R12 ], RAX						; store them at m + 1 (the word 'before' the other up to eight qwords)
				;
				XOR				RAX, RAX
				MOV				RCX, -1
@@:
				TEST			Q_PTR [ R12 ] [ RAX * 8 ], RCX		; R12 currently has addr of beginning of enumerator, 
				JNZ				@F
				INC				RAX
				CMP				RAX, 8
				JLE				@B
@@:
				LEA				R12, Q_PTR [ R12 ] [ RAX * 8 ]		; revise beginning address of working numerator with address of first non-zero qword
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

; Step D3: Calculate  q^
D3:
				MOVZX			RCX, jIdx
				MOV				RDX, Q_PTR [ R12 ] [ RCX * 8 ]
				INC				RCX
				MOV				RAX, Q_PTR [ R12 ] [ RCX * 8 ]
				DIV				nDiv
				MOV				qHat, RAX
				MOV				rHat, RDX

; Step D4: Multiply trial quotient digit by full normalized divisor, then subtract from working copy of numerator (tricky alignment issues)
D4:
				LEA				RCX, qdiv
				Zero512			RCX
				LEA				RCX, qdiv [ 8 * 8 ]					; qdiv is 16 qwords. Last eight are answer
				LEA				RDX, qdiv [ 7 * 8 ]					; Nr 7 will be overflow (if any)
				LEA				R8, normdivisor						; the normalized 8 qword divisor
				MOV				R9, qHat							; times qhat
				CALL			mult_uT64							; multiply divisor by trial quotient digit (qHat)

; a little intricate here: 	Need the starting address of the two operands, the shorter of the two (remaining) lengths
; explaining the intricacies: the leading digit of the multiplied number needs to line up with the leading digit of the enumerator @ [ R12 ]
; the multiply may have added a digit (a qword). The added digit may be in the answer, or may be in the overflow
; subtract the result of the multiply from the remaining (current) numerator

; Calculate length of subtraction (sublen)
				MOVZX			EAX, nDim							; number of words in divisor
				MOVZX		    ECX, mDim
				SUB				CX, jIdx							; remaining words in numerator
				CMP				RAX, RCX
				CMOVG			RAX, RCX							; sublen = min(nDim, mDim - jIdx)
				MOV				R8, RAX								; R8 = sublen
				MOV				sublen, RAX	

; Set up pointers for subtraction
				LEA				RAX, [ 8 ]							; RAX = 8
				SUB				RAX, R8						
				LEA			    RCX, qdiv [ RAX * 8 ]				; RCX = start of relevant qdiv
				MOVZX			RAX, jIdx
				LEA				RAX, [ RAX ][ R8 ] -1
				LEA			    RDX, currnumerator [ RAX * 8 ]		; RDX = start of relevant currnumerator

; Save for possible add-back
				MOV				addbackRDX, RDX
				MOV			    addbackR11, RCX

; Call subtraction subroutine
				CALL    @sub

; Step D5: Test remainder
D5:
				MOVZX			RAX, jIdx
				MOV				RCX, qHat
				MOV				quotient [ RAX * 8 ], RCX			; Set quotient digit [ j ] 
				JNC				D7									; Carry (from above) indicates result of subtract went negative, need D6 add back, else on to D7

; Step D6: Add Back
D6:
				DEC				quotient [ RAX * 8 ]				; adjust quotient digit
				MOV				R8, sublen
				MOV				RCX, addbackR11
				MOV				RDX, addbackRDX
				CALL			@addback
				JMP				 D7

; Step D7: Loop on j
D7:
				INC				jIdx								; increase loop counter
				CMP				jIdx, 8								; done?
				JLE				D3									; no, loop to D3

; Step D8: Un Normalize:
D8UnNormalize:
				MOV				RCX, savedRDX						; reduced working numerator is now the remainder
				LEA				RDX, currnumerator [ 8 * 8 ]		; shifted result to callers remainder
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
				LEA				EAX, [ retcode_neg_one ]
				JMP				cleanupwretcode

divbyone:
				MOV				RCX, savedRCX						; callers quotient
				MOV				R8,  savedR8						; callers dividend
				Copy512			RCX, R8								; copy dividend to quotient
				MOV				RDX, savedRDX						; callers remainder	
				Zero512			RDX									; remainder is zero
				JMP				cleanupret

numtoremain:	MOV				R8, savedR8							; callers dividend
				MOV				RDX, savedRDX						; callers remainder
				Copy512			RDX, R8
				JMP				cleanupret
; subroutine (called) to subtract n digits at [RCX] from [RDX] count of digits in R8
@sub:			CLC
@@:				TEST			R8,  R8
				JZ				@e1
				MOV				RAX, Q_PTR [ RCX ] [ R8 * 8 ]
				SBB				Q_PTR [ RDX ] [ R8 * 8 ], RAX
				DEC				R8
				JMP				@B
@e1:			RET

; subroutine (called) to add (back) n digits at [RCX] to [RDX] count of digits in R8
@addback:		CLC
@@:				TEST			R8, R8
				JZ				@e2
				MOV				RAX, Q_PTR [ RCX ] [ R8 * 8 ]
				ADC				Q_PTR [ RDX ] [ R8 * 8 ], RAX
				DEC				R8
				JMP				@B
@e2:			RET

div_u			ENDP
				Other_Exit		div_u, ui512


;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			EXTERNDEF		div_uT64:PROC				; s16 div_uT64( u64* quotient, u64* remainder, u64* dividend, u64 divisor)
;			div_uT64		-	divide 512 bit dividend by 64 bit divisor, giving 512 bit quotient and 64 bit remainder
;			Prototype:		-	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64 divisor);
;			quotient		-	Address of 8 QWORDS to store resulting quotient (in RCX)
;			remainder		-	Address of QWORD for resulting remainder (in RDX)
;			dividend		-	Address of 8 QWORDS dividend (in R8)
;			divisor			-	Value of 64 bit divisor (in R9)
;			returns			-	0 for success, -1 for attempt to divide by zero, (GP_Fault) for mis-aligned parameter address
;
;			Regs with contents destroyed, not restored: RAX, RDX, R10 (each considered volitile, but caller might optimize on other regs)

				Other_Entry		div_uT64, ui512
div_uT64		PROC			PUBLIC
				CheckAlign		RCX									; (out) Quotient
				CheckAlign		R8									; (in) Dividend

; Test divisor for divide by zero				
				TEST			R9, R9
				JZ				@@DivByZero

; DIV instruction (64-bit) uses RAX and RDX. Need to move RDX (addr of remainder) out of the way; start it off with zero
				LEA				R10,  [ RDX ]						; save addr of callers remainder
				XOR				RDX, RDX

; FOR EACH index of 0 thru 7: get qword of dividend, divide by divisor, store qword of quotient
				FOR				idx, < 0, 1, 2, 3, 4, 5, 6, 7 >
				MOV				RAX, Q_PTR [ R8 ] [ idx * 8 ]		; dividend [ idx ] -> RAX
				DIV				R9									; divide by divisor in R9 (as passed)
				MOV				Q_PTR [ RCX ] [ idx * 8 ], RAX		; quotient [ idx ] <- RAX ; Note: remainder in RDX for next divide
				ENDM

; Last (least significant qword) divide leaves a remainder, store it at callers remainder
				MOV				Q_PTR [ R10 ], RDX					; remainder to callers remainder
				XOR				RAX, RAX							; return zero
@@exit:			
				RET

; Exception handling, divide by zero
@@DivByZero:
				Zero512			RCX									; Divide by Zero. Could throw fault, but returning zero quotient, zero remainder
				XOR				RAX, RAX
				MOV				Q_PTR [ R10 ] , RAX
				LEA				EAX, [ retcode_neg_one ]			; return error (div by zero)
				JMP				@@exit

div_uT64		ENDP
				Other_Exit		div_uT64, ui512

				END