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

mult_u			PROC			PUBLIC
				LOCAL			padding1 [ 8 ] : QWORD
				LOCAL			product [ 16 ] : QWORD
				LOCAL			savedRBP : QWORD
				LOCAL			savedRCX : QWORD, savedRDX : QWORD, savedR10 : QWORD, savedR11 : QWORD, savedR12 : QWORD
				LOCAL			plierl : WORD						; low limit index of of multiplier (7 - first non-zero)
				LOCAL			candl : WORD						; low limit index of multiplicand
				LOCAL			padding2 [ 16 ] : QWORD
mult_u_ofs		EQU				padding2 + 64 - padding1			; offset is the size of the local memmory declarations
				CreateFrame		300h, savedRBP
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
				CMP				EAX, -1								; multiplicand = 0? exit with product = 0
				JE				@@zeroandexit
				CMP				EAX, 0								; multiplicand = 1?	exit with product = multiplier
				MOV				RDX, R9								; address of multiplier (to be copied to product)
				JE				@@copyandexit
				SHR				AX, 6								; divide by 64 to get Nr words
				MOV				RCX, 7
				SUB				CX, AX								; subtract from 7 to get starting (high order) begining index
				MOV				candl, CX							; save off multiplicand index lower limit (eliminate multiplying leading zero words)

; Examine multiplier, save dimensions, handle edge cases of zero or one
				MOV				RCX, R9								; examine multiplier
				CALL			msb_u
				CMP				EAX, -1								; multiplier = 0? exit with product = 0
				JE				@@zeroandexit
				CMP				EAX, 0								; multiplier = 1? exit with product = multiplicand
				MOV				RDX, R8								; address of multiplicand (to be copied to product)
				JE				@@copyandexit
				SHR				AX, 6
				MOV				RCX, 7
				SUB				CX, AX
				MOV				plierl, CX							; save off multiplier index lower limit (eliminate multiplying leading zero words)

; In heap / frame / stack reserved memory, clear 16 word area for overflow/product; set up indexes for loop
				LEA				RCX, product [ 0 ]
				Zero512			RCX									; clear working copy of overflow, need to start as zero, results are added in
				LEA				RCX, product[ 8 * 8 ]
				Zero512			RCX									; clear working copy of product (they need to be contiguous, so using working copy, not callers)
				MOV				R11, 7								; index for multiplier (reduced until less then saved plierl)
				MOV				R12, 7								; index for multiplicand (reduced until less than saved candl)

; multiply loop: an outer loop for each non-zero word of multiplicand, with an inner loop for each non-zero word of multiplier
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
@@:																	; next word of multiplicand
				DEC				R12
				CMP				R12W, candl
				JGE				@@multloop
				MOV				R12, 7
				DEC				R11
				CMP				R11W, plierl
				JGE				@@multloop							; next word of multiplier

; copy working product/overflow to callers product / overflow
				MOV				RCX, savedRCX
				LEA				RDX, product [ 8 * 8 ]
				Copy512			RCX, RDX						; copy working product to callers product
				MOV				RCX, savedRDX
				LEA				RDX, product [ 0 ]
				Copy512			RCX, RDX						; copy working overflow to callers overflow

; restore regs, release frame, return
@@exit:			
				MOV				R10, savedR10
				MOV				R11, savedR11
				MOV				R12, savedR12					; restore any non-volitile regs used
				ReleaseFrame	savedRBP
				XOR				RAX, RAX						; return zero
				RET

; zero callers product and overflow
@@zeroandexit:
				MOV				RCX, savedRCX
				Zero512			RCX
				MOV				RCX, savedRDX
				Zero512			RCX
				JMP				@@exit

; multiplying by 1: zero overflow, copy the non-one to the product
@@copyandexit:
				MOV				RCX, savedRDX					; address of passed overflow
				Zero512			RCX 							; zero it
				MOV				RCX, savedRCX					; copy (whichever: multiplier or multiplicand) to callers product
				Copy512			RCX, RDX						; RDX "passed" here from whomever jumped here (either multiplier, or multiplicand in RDX)
				JMP				@@exit							; and exit
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
mult_uT64	PROC			PUBLIC

			LOCAL			padding1 [ 8 ] :QWORD
			LOCAL			product [ 8 ] :QWORD
			LOCAL			savedRBP :QWORD, savedRCX :QWORD, savedRDX :QWORD 
			LOCAL			overflow :QWORD
			LOCAL			padding2 [ 8 ] :QWORD
mult64_oset	EQU				padding2 + 64 - padding1

			CreateFrame		200h, savedRBP
			MOV				savedRCX, RCX
			MOV				savedRDX, RDX

			LEA				RCX, product
			Zero512			RCX								; clear working copy of product and overflow
			XOR				RAX, RAX
			MOV				overflow, RAX
			;
	
			MOV				RAX, [ R8 ] + [ 7 * 8 ]			; multiplicand 8th qword
			MUL				R9								; times multiplier
			ADD				product [ 7 * 8 ], RAX			; to working product 8th word
			ADC				product [ 6 * 8 ], RDX			; 'overflow' to 7th qword of working product
			MOV				RAX, [ R8 ] + [ 6 * 8 ]			; 7th
			MUL				R9
			ADD				product [ 6 * 8 ], RAX			
			ADC				product [ 5 * 8 ], RDX
			MOV				RAX, [ R8 ] + [ 5 * 8 ]			; 6th
			MUL				R9
			ADD				product [ 5 * 8 ], RAX			
			ADC				product [ 4 * 8 ], RDX
			MOV				RAX, [ R8 ] + [ 4 * 8 ]			; 5th
			MUL				R9
			ADD				product [ 4 * 8 ], RAX			
			ADC				product [ 3 * 8 ], RDX
			MOV				RAX, [ R8 ] + [ 3 * 8 ]			; 4th
			MUL				R9
			ADD				product [ 3 * 8 ], RAX			
			ADC				product [ 2 * 8 ], RDX
			MOV				RAX, [ R8 ] + [ 2 * 8 ]			; 3rd
			MUL				R9
			ADD				product [ 2 * 8 ], RAX			
			ADC				product [ 1 * 8 ], RDX
			MOV				RAX, [ R8 ] + [ 1 * 8 ]			; 2nd
			MUL				R9
			ADD				product [ 1 * 8 ], RAX			
			ADC				product [ 0 * 8 ], RDX
			MOV				RAX, [ R8 ] + [ 0 * 8 ]			; 1st
			MUL				R9
			ADD				product [ 0 * 8 ], RAX
			ADC				overflow, RDX					; last qword overflow is also the operation overflow

			MOV				RCX, savedRCX					; send results back to caller
			LEA				RDX, product
			Copy512			RCX, RDX
			MOV				RAX, overflow
			MOV				RDX, savedRDX
			MOV				Q_PTR [ RDX ], RAX

;			release frame, return
			ReleaseFrame	savedRBP
			XOR				RAX, RAX						; return zero
			RET
mult_uT64	ENDP

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
div_u		PROC			PUBLIC

			LOCAL			padding1 [ 8 ] : QWORD
			LOCAL			currnumerator [ 16 ] : QWORD
			LOCAL			qdiv [ 8 ] : QWORD			
			LOCAL			quotient [ 8 ] : QWORD
			LOCAL			normdivisor [ 8 ] : QWORD
			LOCAL			savedRBP : QWORD
			LOCAL			savedRCX : QWORD, savedRDX : QWORD, savedR8 : QWORD, savedR9 : QWORD
			LOCAL			savedR10 : QWORD, savedR11 : QWORD, savedR12 : QWORD
			LOCAL			qHat : QWORD
			LOCAL			rHat : QWORD
			LOCAL			qovf : QWORD
			LOCAL			currenumbegin : QWORD
			LOCAL			normf : WORD
			LOCAL			limEnumIdx : WORD
			LOCAL			jIdx: WORD
			LOCAL			dimM: WORD
			LOCAL			dimN: WORD
			
			LOCAL			padding2 [ 16 ] : QWORD
div_oset	EQU				padding2 + 64 - padding1

			CreateFrame		320h, savedRBP
			MOV				savedRCX, RCX
			MOV				savedRDX, RDX
			MOV				savedR8, R8
			MOV				savedR9, R9
			MOV				savedR10, R10
			MOV				savedR11, R11
			MOV				savedR12, R12
			;
			Zero512			RCX							; zero callers quotient
			MOV				RCX, RDX
			Zero512			RCX							; zero callers remainder
			LEA				RCX, quotient
			Zero512			RCX							; zero working quotient
			;
			MOV				RCX, savedR9				; divisor
			CALL			msb_u						; most significant bit
			CMP				AX, 0						; msb < 0? 
			JL				divbyzero					; divisor is zero, abort
			CMP				AX, 64						; divisor only one 64-bit word?
			JGE				mbynDiv						; no, do divide of m digit by n digit
			;	divide of m 64-bit digits by one 64 bit divisor
			MOV				RCX, savedRCX				; set up parms for call to div by 64bit: RCX - addr of quotient
			MOV				RDX, savedRDX				; RDX - addr of remainder
			MOV				R8, savedR8					; R8 - addr of dividend
			MOV				RAX, savedR9
			MOV				R9, Q_PTR [RAX + 7 * 8 ]	; R9 - value of 64 bit divisor
			CALL			div_uT64
			MOV				RDX, savedRDX				; move 64 bit remainder to last word of 8 word remainder
			MOV				RAX, Q_PTR [ RDX ]
			MOV				Q_PTR [ RDX + 7 * 8 ], RAX
			XOR				RAX, RAX					; clear first word of remainder (where 64 bit divide put it)
			MOV				Q_PTR [ RDX ], RAX
			JMP				cleanupret

;
;			Going to divide an 'm' digit dividend (u), by an 'n' digit divisor (v)
;				See Knuth, The Art of Computer Programming, Volume 2, Algorithm D, Pages 272-278
mbynDiv:
			MOV				dimN, AX					; still have divisor msb in AX
			SHL				dimN, 6						; div msb by 6 to get msq (most significant qword) aka 'n'
			MOV				normf, 64
			SUB				normf, AX					; Nr bits to get leading divisor bit to msb saved at normf			
			XOR				RAX, RAX
			MOV				dimM, AX
@@:
			INC				dimM
			CMP				dimM, 8
			JE				cleanupret					; dividend is zero, both quotient and remainder already set to zero, so returm
			CMP				RAX, [ R8 ]
			LEA				R8, 8 [ R8 ]
			JZ				@B
			DEC				dimM						; now have dimensions of divisor (v) which is dimN, and dividend (u) which is dimM

;			Step D1: Normalize	
				
			MOV				RDX, savedR9				; callers divisor
			LEA				RCX, normdivisor			; local copy of divisor, normalized
			MOV				R8W, normf
			CALL			shl_u						; by shifting until MSB is high bit
			;
			MOV				RDX, savedR8				; callers dividend
			LEA				RCX, currnumerator [ 8 * 8 ]; starting numerator is the normalized supplied dividend
			MOV				R8W, normf
			CALL			shl_u
			MOV				RDX, savedR8
			LEA				RCX, currnumerator [ 0 * 8 ]
			MOV				R8W, 64
			SUB				R8W, normf
			CALL			shr_u						; now have up to 16 word bit-shifted dividend in 'enumerator'
			LEA				RCX, currnumerator
@@:
			MOV				currenumbegin, RCX			; set address of the first non-zero word of the dividend (enumerator)
			MOV				RDX, [ RCX ]
			TEST			RDX, RDX
			LEA				RCX, [ RCX + 8 ]
			JZ				@B
			MOVZX			RCX, normf
			SHR				CX, 4
			MOV				limEnumIdx, CX
;			Step D2: Initialize
			XOR				RAX, RAX
			MOV				jIdx, AX

;			Step D3: Calculate  q^
D3:
			XOR				RDX, RDX					; DIV takes 128 bits, RDX the high 64, RAX the low
			MOVZX			RAX, jIdx
			MOV				RCX, currenumbegin
			LEA				RCX, [ RCX + RAX * 8 ]
			MOV				RDX, [ RCX ]
			MOV				RAX, [ RCX + 8 ]
			MOV				RCX, normdivisor
			DIV				RCX
			MOV				qHat, RAX
			MOV				rHat, RDX
;			test q^
@retest:
			TEST			RAX, RAX					; q^ = b? (2^64)
			JZ				@adj
			MUL				normdivisor [ 1 * 8 ]
			MOVZX			RCX, jIdx
			MOV				R8, currenumbegin
			MOV				RCX, [ R8 + RCX * 8 + 16]
			ADD				RCX, rHat
			JC				@5
			CMP				RAX, RCX
			JG				@adj
@5:			CMP				RDX, 1
			JG				@adj
			CMP				RAX, RCX
			JLE				D4
@adj:
			MOV				RAX, qHat
			DEC				RAX
			MOV				qHat, RAX
			MOV				RAX, RCX
			ADD				RAX, rHat
			MOV				rHat, RAX
			MOV				RAX, qHat
			JNC				@retest

;			Step D4: Multiply and Subtract
D4:
			LEA				RCX, qdiv
			LEA				RDX, qovf
			LEA				R8, normdivisor
			MOV				R9, qHat
			CALL			mult_uT64					; multiply divisor by trial quotient digit (qHat)
			MOV				RAX, qovf
			TEST			RAX, RAX
			JNZ				cleanupret
			LEA				RCX, currnumerator
			LEA				RDX, currnumerator
			LEA				R8, qdiv
			CALL			sub_u
			LEA				R12, 8 [ R12 ]
			CMP				R12, 8 * 8
			JL				D3

D8UnNormalize:
			MOV				RCX, savedRDX				; reduced working numerator is now the remainder
			LEA				RDX, currnumerator			; copy to callers remainder
			MOV				R8W, normf
			CALL			shr_u
			;
			MOV				RCX, savedRCX				; copy working quotient to callers quotient
			LEA				RDX, quotient
			Copy512			RCX, RDX
cleanupret:
			MOV				R12, savedR12
			MOV				R11, savedR11
			MOV				R10, savedR10
			MOV				R9,  savedR9
			MOV				R8,  savedR8
			MOV				RDX, savedRDX
			MOV				RCX, savedRCX				; restore parameter registers back to "as-called" values
			ReleaseFrame	savedRBP
			XOR				RAX, RAX					; return zero
exit:
			RET
divbyzero:
			MOV				AX, -1
			JMP				exit

div_u		ENDP

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

			OPTION			PROLOGUE:none
			OPTION			EPILOGUE:none
div_uT64	PROC			PUBLIC

			TEST			R9, R9
			JZ				@@DivByZero
			;
			MOV				R10, RDX
			XOR				RDX, RDX
			MOV				RAX, Q_PTR [ R8 + 0 * 8 ]		; Dividend first word
			DIV				R9								; Divisor
			MOV				Q_PTR [ RCX + 0 * 8 ], RAX		; Quotient to callers quotient first word; Div moved remainder to RDX
			MOV				RAX, Q_PTR [ R8 + 1 * 8 ]		; 2nd
			DIV				R9
			MOV				Q_PTR [ RCX + 1 * 8 ], RAX
			MOV				RAX, Q_PTR [ R8 + 2 * 8 ]		; 3rd
			DIV				R9
			MOV				Q_PTR [ RCX + 2 * 8 ], RAX
			MOV				RAX, Q_PTR [ R8 + 3 * 8 ]		; 4th
			DIV				R9
			MOV				Q_PTR [ RCX + 3 * 8 ], RAX
			MOV				RAX, Q_PTR [ R8 + 4 * 8 ]		; 5th
			DIV				R9
			MOV				Q_PTR [ RCX + 4 * 8 ], RAX
			MOV				RAX, Q_PTR [ R8 + 5 * 8 ]		; 6th
			DIV				R9
			MOV				Q_PTR [ RCX + 5 * 8 ], RAX
			MOV				RAX, Q_PTR [ R8 + 6 * 8 ]		; 7th
			DIV				R9
			MOV				Q_PTR [ RCX + 6 * 8 ], RAX		; 8th and final
			MOV				RAX, Q_PTR [ R8 + 7 * 8 ]
			DIV				R9
			MOV				Q_PTR [ RCX + 7 * 8 ], RAX
			MOV				Q_PTR [ R10 ], RDX				; remainder to callers remainder
			XOR				RAX, RAX						; return zero
@@exit:			
			RET

;
@@DivByZero:
			Zero512			RCX								; Divide by Zero. Could throw fault, but returning zero quotient, zero remainder
			XOR				RAX, RAX
			MOV				Q_PTR [ R10 ] , RAX				;
			MOV				AX, -1							; return error (div by zero)
			JMP				@@exit
div_uT64	ENDP

			END