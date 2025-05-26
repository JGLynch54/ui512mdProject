.NOLIST
;
;			ui512mdMacros
;
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			File:			ui512mdMacros.inc
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
;--------------------------------------------------------------------------------------------------------------------------------------------------------------
.LIST
IFNDEF			ui512mdMacros_INC
ui512mdMacros_INC EQU			<1>

;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;			signatures (from ui512md.asm)

; //			mult_uT64		-	multiply 512 bit multiplicand by 64 bit multiplier, giving 512 product, 64 bit overflow
; //			Prototype:		-	void mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);
EXTERNDEF		mult_uT64:PROC	;	void mult_uT64( u64* product, u64* overflow, u64* multiplicand, u64 multiplier);

; //			mult_u			-	multiply 512 multiplicand by 512 multiplier, giving 512 product, overflow
; //			Prototype:		-	void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier);
EXTERNDEF		mult_u:PROC		;	void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier);

; //			div_uT64		-	divide 512 bit dividend by 64 bit bit divisor, giving 512 bit quotient and 64 bit remainder
; //			Prototype:		-	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64 divisor,);
EXTERNDEF		div_uT64:PROC	;	s16 div_uT64( u64* quotient, u64* remainder, u64* dividend, u64 divisor);

; //			div_u			-	divide 512 bit dividend by 512 bit divisor, giving 512 bit quotient and remainder
; //			Prototype:		-	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor);
EXTERNDEF		div_u:PROC		;	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor);

; //			reg_verify		-	save non-volatile regs for verification (debug)
; //			Prototype		-	void reg_verify ( u64* reg struct)
EXTERNDEF		reg_verify:PROC	;	void reg_verify ( u64* reg struct)

;--------------------------------------------------------------------------------------------------------------------------------------------------------------


CreateFrame		MACRO			argsize, argsavename
;
;			set up frame to save regs, and to create aligned working memory for scratch variables
;			argsize is the amount of space to be made on the stack for locals and padding (at least 40h on each end)
;			argsavename is the name of the variable, within the LOCAL declarations, QWORD, for the RBP to be saved, and restored from
;			No prologue, epilogue should be used. LOCAL declarations immediately after PROC statement, then this macro
;			Note: LOCAL variables will START on a 64 byte aligned address space. Your order and sizes of your variables will be in sequence as declared
;			and thus provide no assurance of alignment other than what your declarations imply. In other words, declaring a byte var at the beginning
;			throws everyting after that off by a byte. OK, not exacly, the asm will align by data type (eg, QWORD on 8 byte), but there is no way to get back
;			to 64 byte alignment, so declare those first, then QWORD, then DWORD, etc. Or, count your declares such that you get what/where you want.
;			example:
;
;somename	PROC
;			LOCAL		padding1[8]:QWORD				; warning: do not touch, initialize, or use padding. (on either end)
;			LOCAL		somelocal:ZMMWORD				; a 512 bit, 64 byte aligned var, ready for aligned load/store into ZMM (AVX2) register
;			LOCAL		some local variable declarions, some more, and some more
;			LOCAL		and some more
;			LOCAL		savedRBP:QWORD					; you might have other reg save space as well
;			LOCAL		padding2[8]:QWORD				; after your return statement, before ENDP, might LEA padding1 and padding2 to eliminate warning (unreferenced variables)
;			CREATEFRAME 200h, savedRBP					; (200h assumes 118h in those "some local variable declarations", adjust as necessary)
;
;			Use only one return from the PROC, and immediately before return, use ReleaseFrame macro, giving name of where RBP is saved
;
				PUSH			RBP
				MOV				RBP, RSP
				AND				RSP, -8
				SUB				RSP, argsize + 64					; make a gap between old stack pointer and current stack pointer for use as "LOCAL", adjust size if changes made to locals
				MOV				RAX, -64							; Round it (down), to make it, and local vars, align on 64 byte address (necessary for aligned ZMM load/store)
				AND				RAX, RBP							; note: "padding" variables must not be used. They are in areas where we have just rounded down, or in stack space for those we call
				XCHG			RAX, RBP							; RBP now points to top of gap between old stack pointer and current stack pointer, aligned on 64
				MOV				argsavename, RAX					; LOCAL variables now usable, using negative offsets from the new RBP value. Must restore RBP, RSP at exit (Use RELEASEFRAME)
				ENDM
;--------------------------------------------------------------------------------------------------------------------------------------------------------------


ReleaseFrame	MACRO			argsavename
;			release memory set up by createframe macro
;			restores RSP, and RBP to as-called values
;			after these instructions are executed, LOCAL variables can NOT be accessed
;			This needs to be done to restore the stack correctly, but can be done only once
;			Strongly suggest doing this just prior to return instruction, and that there is only one return instruction from the PROC
				MOV				RSP, argsavename					; restore unadjusted / unrounded (the as when called) stack pointer (eliminating LOCAL storage) 
				POP				RBP									; restore base pointer for caller
				ENDM

;--------------------------------------------------------------------------------------------------------------------------------------------------------------
;==========================================================================================
;           Notes on x64 calling conventions        aka "fast call"
; ref: https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170
; The first four parameters are passed in registers: RCX, RDX, R8, R9 if integer or address
; if floating point XMM0L, XMM1L, XMM2L, XMM3L
;===========================================================================================
;
;===========================================================================================
; RAX, RCX, RDX, R8, R9, R10, R11 are considered volatile, and do not need to be saved
; XMM0, YMM0, ZMM0 and  ..1, ..2, ..3, ..4, and ..5 are considered volatile,
;	and do not need to be saved
;  ZMM16 to ZMM31: volatile, also do not need to be zeroed to resume full clock speeds
;
; R12, R13, R14, R15, RDI, RSI, RBX, RBP, RSP are non-volatile and if used, must be restored
; XMM, YMM, and ZMM ..6 thru 15 are non-volatile and if used, must be restored
;
; A "leaf" function is one that does not call and does not change non volatile registers
; leaf functionss therefore do not need frame, prolog or epilog
;
;===========================================================================================

ENDIF