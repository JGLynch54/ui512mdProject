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
;			EXTERNDEF		mult_u:PROC					;	void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier)

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
			LOCAL			savedRCX:QWORD, savedRDX:QWORD, savedRBP:QWORD, savedR8:QWORD
			LOCAL			savedR9:QWORD, savedR10:QWORD, savedR11:QWORD, savedR12:QWORD
			LOCAL			plierWC:WORD, candWC:WORD
			LOCAL			padding2[8]:QWORD

			CreateFrame		240h, savedRBP
			MOV				savedRCX, RCX
			MOV				savedRDX, RDX
			MOV				savedR8, R8
			MOV				savedR9, R9
			MOV				savedR10, R10
			MOV				savedR11, R11
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
			MOV				candWC, AX					; save off word count for multiplicand
			;
			MOV				RCX, R9						; examine multiplier
			CALL			msb_u
			CMP				AX, -1						; multiplier = 0? exit with product = 0
			JE				zeroandexit
			CMP				AX, 0						; multiplier = 1? exit with product = multiplicand
			MOV				RDX, R8						; address of multiplicand (to be copied to product)
			JE				copyandexit
			SHR				AX, 6
			MOV				plierWC, AX					; save off word count for multiplier
			JMP				mult
			;
zeroandexit:
			MOV				RCX, savedRCX
			Zero512			RCX
			MOV				RCX, savedRDX
			Zero512			RCX
			JMP				exitmultnocopy
copyandexit:
			MOV				RCX, savedRDX				; address of passed overflow
			Zero512			RCX 						; zero it
			MOV				RCX, savedRCX				; copy (whichever: multiplier or multiplicand) to callers product
			Copy512			RCX, RDX
			JMP				exitmultnocopy				; and exit
mult:
			LEA				RCX, product [ 0 ]
			Zero512			RCX							; clear working copy of overflow
			LEA				RCX, product[ 8 * 8 ]
			Zero512			RCX							; clear working copy of product (they need to be contiguous, so using working copy, not callers)
			;
			MOV				R11, savedR9				; address of callers multiplier
			MOV				R8, 7
			SUB				R8W, plierWC				 
			MOV				plierWC, R8W
			MOV				R8, 7						; R8 : index for multiplier, plierWC : low limit for index
			;
			MOV				R12, savedR8				; address of callers multiplicand
			MOV				R9, 7
			SUB				R9W, candWC
			MOV				candWC, R9W
			MOV				R9, 7						; R9 : index for multiplicand; candWC : low limit for index
			;
multloop:
			MOV				R10, R8
			ADD				R10, R9
			INC				R10							; R10 : index for product/overflow
			;
			MOV				RAX, [ R12 ] + [ R9 * 8 ]
			MUL				Q_PTR [ R11 ] + [ R8 * 8 ]
			ADD				product [ R10 * 8 ], RAX
			;
			DEC				R10							; preserves carry flag
addcarryloop:
			ADC				product [ R10 * 8 ], RDX
			MOV				RDX, 0						; again, preserves carry flag
			JNC				nextcand
			DEC				R10
			JGE				addcarryloop
nextcand:
			DEC				R9
			CMP				R9W, candWC
			JGE				multloop
			MOV				R9, 7
			DEC				R8
			CMP				R8W, plierWC
			JGE				multloop
;			copy working product/overflow to callers product / overflow
			MOV				RCX, savedRCX
			LEA				RDX, product [ 8 * 8 ]
			Copy512			RCX, RDX					; copy working product to callers product
			MOV				RCX, savedRDX
			LEA				RDX, product [ 0 ]
			Copy512			RCX, RDX					; copy working overflow to callers overflow
;			restore regs, release frame, return
exitmultnocopy:			
			MOV				R12, savedR12
			MOV				R11, savedR11
			MOV				R10, savedR10
			MOV				R9, savedR9
			MOV				R8, savedR8
			MOV				RDX, savedRDX
			MOV				RCX, savedRCX				; restore parameter registers back to "as-called" values
			ReleaseFrame	savedRBP
			XOR				RAX, RAX					; return zero
			RET
			LEA				RAX, padding1				; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2
mult_u		ENDP

;			EXTERNDEF		mult_uT64:PROC				;	void mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);

;			mult_uT64		-	multiply 512 bit multiplicand by 64 bit multiplier, giving 512 product, 64 bit overflow
;			Prototype:		-	void mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);
;			product			-	Address of 8 QWORDS to store resulting product (in RCX)
;			overflow		-	QWORD for resulting overflow (in RDX)
;			multiplicand	-	Address of 8 QWORDS multiplicand (in R8)
;			multiplier		-	multiplier QWORD (in R9)
;			returns			-	nothing (0)

			OPTION			PROLOGUE:none
			OPTION			EPILOGUE:none
mult_uT64	PROC			PUBLIC

			LOCAL			padding1 [ 8 ] :QWORD
			LOCAL			product [ 8 ] :QWORD
			LOCAL			overflow :QWORD
			LOCAL			savedRCX :QWORD, savedRDX :QWORD, savedRBP :QWORD
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
			ADD				product [ 7 * 8], RAX			; to working product 8th word (don't need add as no previous overflow)')
			ADC				product [ 6 * 8 ], RDX			; 'overflow' to 7th qword of working product
			;
			MOV				RAX, [ R8 ] + [ 6 * 8 ]
			MUL				R9
			ADD				product [ 6 * 8 ], RAX
			ADC				product [ 5 * 8 ], RDX
			;
			MOV				RAX, [ R8 ] + [ 5 * 8 ]
			MUL				R9
			ADD				product [ 5 * 8], RAX
			ADC				product [ 4 * 8 ], RDX
			;
			MOV				RAX, [ R8 ] + [ 4 * 8 ]
			MUL				R9
			ADD				product [ 4 * 8], RAX
			ADC				product [ 3 * 8 ], RDX
			;
			MOV				RAX, [ R8 ] + [ 3 * 8 ]
			MUL				R9
			ADD				product [ 3 * 8], RAX
			ADC				product [ 2 * 8 ], RDX
			;
			MOV				RAX, [ R8 ] + [ 2 * 8 ]
			MUL				R9
			ADD				product [ 2 * 8], RAX
			ADC				product [ 1 * 8 ], RDX
			;
			MOV				RAX, [ R8 ] + [ 1 * 8 ]
			MUL				R9
			ADD				product [ 1 * 8], RAX
			ADC				product [ 0 * 8 ], RDX
			;
			MOV				RAX, [ R8 ] + [ 0 * 8 ]
			MUL				R9
			ADD				product [ 0 * 8], RAX
			;
			ADC				overflow, RDX					; last overflow is really overflow
			;
			MOV				RCX, savedRCX
			LEA				RDX, product
			Copy512			RCX, RDX
			MOV				RAX, overflow
			MOV				RDX, savedRDX
			MOV				Q_PTR [ RDX ], RAX
;			restore regs, release frame, return
			MOV				RDX, savedRDX
			MOV				RCX, savedRCX					; restore parameter registers back to "as-called" values
			ReleaseFrame	savedRBP
			XOR				RAX, RAX						; return zero
			RET

			LEA				RAX, padding1					; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2

mult_uT64	ENDP

;			EXTERNDEF		div_u:PROC					; void div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor)

;			div_u			-	divide 512 bit dividend by 512 bit divisor, giving 512 bit quotient and remainder
;			Prototype:		-	void div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor);
;			quotient		-	Address of 8 QWORDS to store resulting quotient (in RCX)
;			remainder		-	Address of 8 QWORDs for resulting remainder (in RDX)
;			dividend		-	Address of 8 QWORDS dividend (in R8)
;			divisor			-	Address of 8 QWORDs divisor (in R9)
;			returns			-	nothing (0)

			OPTION			PROLOGUE:none
			OPTION			EPILOGUE:none
div_u		PROC			PUBLIC

			LOCAL			padding1[8]:QWORD
			LOCAL			qhat[16]:QWORD
			LOCAL			rhat[8]:QWORD
			LOCAL			quotient[16]:QWORD
			LOCAL			savedRCX:QWORD, savedRDX:QWORD, savedRBP:QWORD, savedR8:QWORD, savedR9:QWORD, savedR10:QWORD, savedR11:QWORD, savedR12:QWORD
			LOCAL			dadj:QWORD					; normalization adjustment
			LOCAL			mbitn:WORD, lbitn:WORD, initn:WORD		; initial count of words of "u", the dividend aka numerator aka the number on top (zero-based, so zero to count-1)
			LOCAL			mbitm:WORD, lbitm:WORD, initm:WORD		; initial count of words of "v", the divisor aka denominator aka the number on the bottom (u / v)
			LOCAL			normd:WORD					; value of normalizing variable d which is set to (radix - 1 over v sub n - 1)
			LOCAL			loopj:WORD					; loop iterator, initially "m"
			LOCAL			shiftadj:WORD				; divisor adjustment to fit into one word
			LOCAL			padding2[8]:QWORD

			CreateFrame		240h, savedRBP
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
			LEA				RCX, quotient [ 8 * 8]
			Zero512			RCX
			LEA				RCX, qhat
			Zero512			RCX
			LEA				RCX, rhat
			Zero512			RCX
			XOR				RAX, RAX
			MOV				dadj, RAX
			MOV				normd, AX
			MOV				loopj, AX
			MOV				shiftadj, AX
			; D1 [Normalize]
			;	Determine dimensions of u and v (dividend and divisor)
			;		Shift if it gets divisor to single or reduced words
			;		Handle edge cases of divisor = 0, or 1, or power of 2
			;	Add an extra word on the front of u
			;	Multiply (again shift) by "d" (chosen as power of two)
			MOV				RCX, savedR8				; get dimensions of dividend
			CALL			msb_u
			MOV				mbitn, AX
			SHR				AX, 6
			MOV				initn, AX
			CALL			lsb_u
			MOV				lbitn, AX
			;
			MOV				RCX, savedR9				; dimensions of divisor
			CALL			msb_u						; count til most significant bit
			MOV				mbitm, AX
			SHR				AX, 6
			MOV				initm, AX
			CALL			lsb_u						; count to least significant bit
			MOV				lbitm, AX
			;
			MOV				AX, mbitm					; divisor edge cases (zero? power of two? one?)
			CMP				AX, 0						; msb < 0? 
			JL				cleanret					; divisor is zero, abort (Note: no indication of fail)
			CMP				AX, lbitm					; msb = lsb? power of two (or one)
			JNZ				notpow2

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
notpow2:
			MOV				AX, mbitm
			XOR				AX, 03fh
			CMP				AX, 60
			JGE				no_d_adj
			MOV				R8W, 63
			SUB				R8W, AX
			MOV				normd, R8W
			JMP				copyw
no_d_adj:
			XOR				RAX, RAX
			MOV				normd, AX

			; copy to work area, adding extra leading word, shifting (mult) by 'd'
copyw:
			; shift is at most 62 bits, so get bits from first word
			MOV				RDX, savedR9				; address of divisor
			MOV				RCX, 7
			SUB				CX, initm
			LEA				RDX, [RDX] + [RCX * 8]


			; D2 [Initialize]
			;	loop counter "j", starting at word count of u ("m")
			MOV				AX, initm
			MOV				loopj, AX
			; D3 [Calculate q]
			;	Initial guess q is qhat, remainder r is rhat
			;	two leading words of u / leading word of v, remainder rhat
			;	test = to radix or greater than second word u times qhat + radix times rhat
			;		adjust if so: decrease qhat by one, add leading word of v to rhat, repeat if rhat < radix

			; D4 [Multiply and subtract]
			;	Multiply qhat by adjusted v
			;	Subtract from adjusted u
			;	Test for negative

			; D5 [Test remainder]
			;	qhat to Q sub j
			;	"go to" add back (D6) if negative, else loop on j [D7]

			; D6 [Add back]
			;	decrease Q sub j by one
			;	Add adjusted v with leading zero to adjusted u sub n +j ...

			; D7 [Loop on j]
			;	decrease j by one
			;	j >= 0 "go to" D3

			; D8 [Un Normalize]
			;	Q is now quotient
			;	Divide remaining u by d for remainder

			;	Clean up and return
			;   
			;
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
			RET
			LEA				RAX, padding1				; reference local variables meant for padding to remove unreferenced variable warning from assembler
			LEA				RAX, padding2
div_u		ENDP

;			EXTERNDEF		div_uT64:PROC				; div_uT64( u64* quotient, u64* remainder, u64* dividend, u64 divisor)

;			div_uT64		-	divide 512 bit dividend by 64 bit divisor, giving 512 bit quotient and 64 bit remainder
;			Prototype:		-	void div_u( u64* quotient, u64* remainder, u64* dividend, u64 divisor);
;			quotient		-	Address of 8 QWORDS to store resulting quotient (in RCX)
;			remainder		-	Address of QWORD for resulting remainder (in RDX)
;			dividend		-	Address of 8 QWORDS dividend (in R8)
;			divisor			-	Value of 64 bit divisor (in R9)

			OPTION			PROLOGUE:none
			OPTION			EPILOGUE:none
div_uT64	PROC			PUBLIC

			TEST			R9, R9
			JZ				DivByZero
			;
			MOV				R10, RDX
			XOR				RDX, RDX
			MOV				RAX, Q_PTR [ R8 + 0 * 8]		; Dividend first word
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
			MOV				Q_PTR [ R10 ] , RDX				; remainder to callers remainder
Exit:
			XOR				RAX, RAX						; return zero
			RET
;
DivByZero:
			Zero512			RCX								; Divide by Zero. Could throw fault, but returning zero quotient, zero remainder
			XOR				RAX, RAX
			MOV				Q_PTR [ R10 ] , RAX				;	(not possible on legit divide)
			JMP				Exit
div_uT64	ENDP

			END