;
;			ui512md
;
;			File:			ui512md.asm
;			Author:			John G. Lynch
;			Legal:			Copyright @2024, per MIT License below
;			Date:			June 20, 2024
;
;			Notes:
;				ui512 is a small project to provide basic operations for a variable type of unsigned 512 bit integer.
;				The basic operations: zero, copy, compare, add, subtract.
;               Other optional modules provide bit ops and multiply / divide.
;				It is written in assembly language, using the MASM (ml64) assembler provided as an option within Visual Studio.
;				(currently using VS Community 2022 17.9.6)
;				It provides external signatures that allow linkage to C and C++ programs,
;				where a shell/wrapper could encapsulate the methods as part of an object.
;				It has assembly time options directing the use of Intel processor extensions: AVX4, AVX2, SIMD, or none:
;				(Z (512), Y (256), or X (128) registers, or regular Q (64bit)).
;				If processor extensions are used, the caller must align the variables declared and passed
;				on the appropriate byte boundary (e.g. alignas 64 for 512)
;				This module is very light-weight (less than 1K bytes) and relatively fast,
;				but is not intended for all processor types or all environments. 
;				Use for private (hobbyist), or instructional,
;				or as an example for more ambitious projects is all it is meant to be.
;
;				ui512b provides basic bit-oriented operations: shift left, shift right, and, or, not,
;               least significant bit and most significant bit.
;
;				This module, ui512md, adds multiply and divide funnctions.
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
			INCLUDE         ui512aMacros.inc
			INCLUDE			ui512bMacros.inc
			INCLUDE			ui512mdMacros.inc

			OPTION			casemap:none
.CODE
;			EXTERNDEF		mult_u:PROC					; void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier)

;			mult_u			-	multiply 512 multiplicand by 512 multiplier, giving 512 product, overflow
;			Prototype:		-	void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier);
;			product			-	Address of 8 QWORDS to store resulting product (in RCX)
;			overflow		-	Address of 8 QWORDS to store resulting overflow (in RDX)
;			multiplicand	-	Address of 8 QWORDS multiplicand (in R8)
;			multiplier		-	Address of 8 QWORDS multiplier (in R9)
;			returns			-	nothing (0)

			OPTION			PROLOGUE:none
			OPTION			EPILOGUE:none
mult_u		PROC			PUBLIC

			LOCAL			padding1[8]:QWORD
			LOCAL			product[16]:QWORD
			LOCAL			savedRBP:QWORD
			LOCAL			savedRCX:QWORD, savedRDX:QWORD
			LOCAL			savedR12:QWORD
			LOCAL			plierll:WORD, candll:WORD
			LOCAL			padding2[8]:QWORD

			CreateFrame		240h, savedRBP
			MOV				savedRCX, RCX
			MOV				savedRDX, RDX
			MOV				savedR12, R12
			;
			MOV				RCX, R8						; examine multiplicand
			CALL			msb_u
			CMP				AX, -1						; multiplicand = 0? exit with product = 0
			JE				zeroandexit
			CMP				AX, 0						; multiplicand = 1?	exit with product = multiplier
			MOV				RDX, R9						; address of multiplier (to be copied to product)
			JE				copyandexit
			SHR				AX, 6
			MOV				RCX, 7
			SUB				CX, AX
			MOV				candll, CX					; save off multiplicand index lower limit (eliminate multiplying leading zero words)
			;
			MOV				RCX, R9						; examine multiplier
			CALL			msb_u
			CMP				AX, -1						; multiplier = 0? exit with product = 0
			JE				zeroandexit
			CMP				AX, 0						; multiplier = 1? exit with product = multiplicand
			MOV				RDX, R8						; address of multiplicand (to be copied to product)
			JE				copyandexit
			SHR				AX, 6
			MOV				RCX, 7
			SUB				CX, AX
			MOV				plierll, CX					; save off multiplier index lower limit (eliminate multiplying leading zero words)
			;
			LEA				RCX, product [ 0 ]
			Zero512			RCX							; clear working copy of overflow, need to start as zero, results are added in
			LEA				RCX, product[ 8 * 8 ]
			Zero512			RCX							; clear working copy of product (they need to be contiguous, so using working copy, not callers)
			;
			MOV				R11, 7						; index for multiplier
			MOV				R12, 7						; index for multiplicand
multloop:
			MOV				R10, R11
			ADD				R10, R12
			INC				R10							; index for product/overflow 
			MOV				RAX, [ R8 ] + [ R12 * 8 ]	; get qword of multiplicand
			MUL				Q_PTR [ R9 ] + [ R11 * 8 ]	; multiply by qword of multiplier
			ADD				product [ R10 * 8 ], RAX
			DEC				R10							; preserves carry flag
propagatecarry:
			ADC				product [ R10 * 8 ], RDX
			MOV				RDX, 0						; again, preserves carry flag
			JNC				nextcand
			DEC				R10
			JGE				propagatecarry
nextcand:
			DEC				R12
			CMP				R12W, candll
			JGE				multloop
			MOV				R12, 7
			DEC				R11
			CMP				R11W, plierll
			JGE				multloop
;			copy working product/overflow to callers product / overflow
			MOV				RCX, savedRCX
			LEA				RDX, product [ 8 * 8 ]
			Copy512			RCX, RDX					; copy working product to callers product
			MOV				RCX, savedRDX
			LEA				RDX, product [ 0 ]
			Copy512			RCX, RDX					; copy working overflow to callers overflow
;			restore regs, release frame, return
exit:			
			MOV				R12, savedR12				; restore any non-volitile regs used
			ReleaseFrame	savedRBP
			XOR				RAX, RAX					; return zero
			RET
zeroandexit:
			MOV				RCX, savedRCX
			Zero512			RCX
			MOV				RCX, savedRDX
			Zero512			RCX
			JMP				exit
copyandexit:
			MOV				RCX, savedRDX				; address of passed overflow
			Zero512			RCX 						; zero it
			MOV				RCX, savedRCX				; copy (whichever: multiplier or multiplicand) to callers product
			Copy512			RCX, RDX
			JMP				exit						; and exit
			LEA				RAX, padding1				; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2
mult_u		ENDP

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
			LOCAL			overflow :QWORD
			LOCAL			savedRBP :QWORD
			LOCAL			savedRCX :QWORD, savedRDX :QWORD 
			LOCAL			padding2 [ 8 ] :QWORD

			CreateFrame		160h, savedRBP
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
			;
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

			LEA				RAX, padding1					; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2

mult_uT64	ENDP

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
div_u2		PROC			PUBLIC

			LOCAL			padding1[ 8 ] :QWORD
			LOCAL			currnumerator [ 8 ] :QWORD
			LOCAL			trialenum [ 8 ] :QWORD
			LOCAL			qdiv [ 8 ] :QWORD			
			LOCAL			quotient[ 8 ] :QWORD
			LOCAL			normdivisor [ 8 ] :QWORD
			LOCAL			savedRBP:QWORD
			LOCAL			savedRCX:QWORD, savedRDX:QWORD, savedR8:QWORD, savedR9:QWORD
			LOCAL			savedR10:QWORD, savedR11:QWORD, savedR12:QWORD
			LOCAL			qovf : QWORD
			LOCAL			div1 : DWORD, div2 : DWORD		; note: must be kept together, and on qword boundary (sometimes accessed together as qword)
			LOCAL			partrem : DWORD
			LOCAL			mbitm:WORD, lbitm:WORD, initm:WORD		; initial count of words of "v", the divisor aka denominator aka the number on the bottom (u / v)
			LOCAL			normf : WORD
			LOCAL			padding2[8]:QWORD

			CreateFrame		280h, savedRBP
			MOV				savedRCX, RCX
			MOV				savedRDX, RDX
			MOV				savedR8, R8
			MOV				savedR9, R9
			MOV				savedR10, R10
			MOV				savedR11, R11
			MOV				savedR12, R12
			;
			LEA				RCX, quotient
			Zero512			RCX
			;
			MOV				RCX, savedR9				; dimensions of divisor
			CALL			msb_u						; count til most significant bit
			CMP				AX, 0						; msb < 0? 
			JL				divbyzero					; divisor is zero, abort
			MOV				mbitm, AX
			SHR				AX, 7
			MOV				R8, 15
			SUB				R8, RAX
			MOV				initm, R8W					; first non-zero word of divisor
			CALL			lsb_u						; count to least significant bit
			MOV				lbitm, AX
			CMP				AX, mbitm					; divisor edge cases (zero? power of two? one?)msb = lsb? power of two (or one)
			JNZ				D1norm
			;	for power of two divisor (or one), shift, with shifted out bits going to remainder, exit
			MOV				RDX, savedR8				; address of dividend
			MOV				RCX, savedRDX				; address of callers remainder
			MOV				R8, 512
			SUB				R8W, lbitm					; shift out all high bits
			CALL			shl_u
			MOV				RDX, savedRDX
			MOV				RCX, RDX
			MOV				R8, 512
			SUB				R8W, lbitm
			CALL			shr_u						; shift back with destination the remainder, eliminating high bits
			MOV				RCX, savedRCX				; callers quotient (even if zero bits shifted, this copies to callers vars)
			MOV				RDX, savedR8				; callers dividend
			XOR				R8, R8
			MOV				R8W, lbitm
			CALL			shr_u						; shift dividend n bits to make quotient
			JMP				cleanret
			; doing divide, normalize, copy to work area
D1norm:
			MOV				AX, 512
			SUB				AX, mbitm
			OR				AX, 31
			MOV				normf, AX
			;
			MOV				RDX, savedR9				; callers divisor
			LEA				RCX, normdivisor
			MOV				R8W, normf
			CALL			shl_u

			MOV				RDX, savedR8				; callers dividend
			LEA				RCX, currnumerator			; starting numerator is the normalized supplied dividend
			MOV				R8W, normf
			CALL			shl_u
; D2 Initialize
			XOR				RAX, RAX
			MOV				EAX, D_PTR [ RDX ]
			MOV				CX, normf
			SHR				EAX, CL						; get leadinng bits of dividend that may have been shifted out
			MOV				div1, EAX
			XOR				RDX, RDX
			MOV				EDX, D_PTR currnumerator
			MOV				div2, EDX					; div1 and div2 (collectiely: div) contains first 2 words of div

			LEA				RDX, normdivisor			; normalized divisor
			XOR				RCX, RCX
			MOV				CX, initm
			XOR				R10, R10
			MOV				R10D, D_PTR [ RDX ] + [ RCX * 4 ]	; highest word of divisor

			XOR				R12, R12					; counter 'j', zero to 15 (once each word)			
D3CalcQhat:
			DIV				R10D
			MOV				D_PTR quotient [ R12 ], EAX
			MOV				partrem, EDX
			;
			MUL				R10
			MOV				R8, Q_PTR div1
			SUB				R8, RAX
			JNC				D4MultAndSub
			;
			ADD				partrem, R10D
			MOV				EAX, D_PTR quotient [ R12 ]
			DEC				RAX
			MOV				D_PTR quotient [ R12 ], EAX
			; 
			MUL				R10
			MOV				R8, Q_PTR div1
			SUB				R8, RAX
			JNC				D4MultAndSub
			;
			ADD				partrem, R10D
			MOV				EAX, D_PTR quotient [ R12 ]
			DEC				RAX
			MOV				D_PTR quotient [ R12 ], EAX
D4MultAndSub:
			LEA				RCX, qdiv
			LEA				RDX, qovf
			LEA				R8, normdivisor
			MOV				R9, RAX
			CALL			mult_uT64
			MOV				RAX, qovf
			TEST			RAX, RAX
			JNZ				errexit
			LEA				RCX, trialenum
			LEA				RDX, currnumerator
			LEA				R8, qdiv
			CALL			sub_u
; D5 Test remainder
			TEST			RAX, RAX
			JNZ				D6AddBack
			LEA				RCX, currnumerator
			LEA				RDX, trialenum
			Copy512			RCX, RDX
			JMP				D7Loop
D6AddBack:
			MOV				EAX, D_PTR quotient [ R12 ]
			DEC				RAX
			MOV				D_PTR quotient [ R12 ], EAX	
D7Loop:
			LEA				R12, 4 [ R12 ]
			CMP				R12, 15 * 4
			JGE				D8UnNormalize
			XOR				RDX, RDX
			XOR				RAX, RAX
				
			JMP				D3CalcQhat
D8UnNormalize:
			MOV				RCX, savedRDX				; reduced working numerator is now the remainder
			LEA				RDX, currnumerator			; copy to callers remainder
			MOV				R8W, normf
			CALL			shr_u
			;
			MOV				RCX, savedRCX				; copy working quotient to callers quotient
			LEA				RDX, quotient
			Copy512			RCX, RDX
cleanret:
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

errexit:
			JMP				cleanret

			LEA				RAX, padding1				; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2
div_u2		ENDP


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

			LOCAL			padding1[ 8 ] :QWORD
			LOCAL			currnumerator [ 8 ] :QWORD
			LOCAL			trialenum [ 8 ] :QWORD
			LOCAL			qdiv [ 8 ] :QWORD			
			LOCAL			quotient[ 8 ] :QWORD
			LOCAL			normdivisor [ 8 ] :QWORD
			LOCAL			savedRBP:QWORD
			LOCAL			savedRCX:QWORD, savedRDX:QWORD, savedR8:QWORD, savedR9:QWORD
			LOCAL			savedR10:QWORD, savedR11:QWORD, savedR12:QWORD
			LOCAL			qovf : QWORD
			LOCAL			leadingdivisor : QWORD
			LOCAL			partrem : QWORD
			LOCAL			normf : WORD
			LOCAL			padding2[8]:QWORD

			CreateFrame		280h, savedRBP
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
			MOV				RCX, savedRCX
			MOV				RDX, savedRDX
			MOV				R8, savedR8
			MOV				RAX, savedR9
			MOV				R9, Q_PTR [RAX + 7 * 8]
			CALL			div_uT64
			MOV				RCX, savedRCX
			MOV				RAX, Q_PTR [ RCX ]
			MOV				Q_PTR [ RCX + 7 * 8 ], RAX
			XOR				RAX, RAX
			MOV				Q_PTR [ RCX ], RAX
			JMP				cleanupret
	
mbynDiv:
			MOV				CX, 511
			XCHG			AX, CX
			SUB				AX, CX						
			AND				AX, 003fh					; Nr leading zero bits in leading non-zero word of divisor
			MOV				normf, AX
						;
			MOV				RDX, savedR9				; callers divisor
			LEA				RCX, normdivisor
			MOV				R8W, normf
			CALL			shl_u

			MOV				RDX, savedR8				; callers dividend
			LEA				RCX, currnumerator			; starting numerator is the normalized supplied dividend
			MOV				R8W, normf
			CALL			shl_u

			MOV				RDX, Q_PTR [ RDX ]
			MOV				CX, 003fH
			SUB				CX, normf
			SHR				RDX, CL						; get leadinng bits of dividend that may have been shifted out
			MOV				partrem, RDX

			LEA				RCX, normdivisor
findleading:			
			MOV				R11, Q_PTR [ RCX ]
			TEST			R11, R11
			JNZ				gotdiv
			LEA				RCX, 8 [ RCX ]				; divisor alredy tested, it is non-zero, no need to check for index out of range
			JMP				findleading
gotdiv:
			MOV				leadingdivisor, R11
			XOR				R12, R12
divloop:
			MOV				RDX, partrem
			MOV				RAX, currnumerator [ R12 ]
			DIV				R11
			MOV				quotient [ R12 ], RAX
			MOV				partrem, RDX
D4MultAndSub:
			LEA				RCX, qdiv
			LEA				RDX, qovf
			LEA				R8, normdivisor
			MOV				R9, RAX
			CALL			mult_uT64
			MOV				RAX, qovf
			TEST			RAX, RAX
			JNZ				errexit
			LEA				RCX, currnumerator
			LEA				RDX, currnumerator
			LEA				R8, qdiv
			CALL			sub_u
			LEA				R12, 8 [ R12 ]
			CMP				R12, 8 * 8
			JL				divloop

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

errexit:
			JMP				cleanupret

			LEA				RAX, padding1				; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2
div_u		ENDP

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
			JZ				DivByZero
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
exit:			
			RET
;
DivByZero:
			Zero512			RCX								; Divide by Zero. Could throw fault, but returning zero quotient, zero remainder
			XOR				RAX, RAX
			MOV				Q_PTR [ R10 ] , RAX				;
			MOV				AX, -1							; return error (div by zero)
			JMP				exit
div_uT64	ENDP

			END