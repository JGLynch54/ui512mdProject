;
IFNDEF						ui512mdMacros_INC
ui512mdMacros_INC EQU			 <1>


;			signatures (from ui512muldiv.asm)
EXTERNDEF	mult_u:PROC					;	void mult_u( u64* product, u64* overflow, u64* multiplicand, u64* multiplier);
EXTERNDEF	mult_uT64:PROC				;	void mult_uT64( u64* product, u64 overflow, u64* multiplicand, u64 multiplier);
EXTERNDEF	div_u:PROC					;	s16 div_u( u64* quotient, u64* remainder, u64* dividend, u64* divisor);
EXTERNDEF	div_uT64:PROC				;	s16 div_uT64( u64* quotient, u64 remainder, u64* dividend, u64 divisor);

CreateFrame	MACRO			argsize, argsavename
;
;			set up frame to save regs, and to create aligned working memory for scratch variables
;			argsize is the amount of space to be made on on the stack for locals and padding (at least 40h on each end)
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
;			CREATEFRAME 200h, savedRBP (200h assumes 118h in those "some local variable declarations", adjust as necessary)
;
;			Use only one return from the PROC, and immediately before return, use RELEASEFRAME macro, giving name of where RBP is saved
;
			PUSH			RBP
			MOV				RBP, RSP
			ADD				RSP, -((argsize / 8) * ( 8 + 1)); make gap between old stack pointer and current stack pointer for use as "LOCAL", adjust size if changes made to locals
			MOV				RAX, -64					; Round it (down), to make it, and local vars, align on 64 byte address (necessary for aligned ZMM load/store)
			AND				RSP, RAX
			AND				RAX, RBP					; note: "padding" variables must not be used. They are in areas where we have just rounded down, or in stack space for those we call
			XCHG			RAX, RBP					; RBP now points to top of gap between old stack pointer and current stack pointer, aligned on 64
			MOV				argsavename, RAX			; LOCAL variables now usable, using negative offsets from the new RBP value. Must restore RBP, RSP at exit (Use RELEASEFRAME)
			ENDM

ReleaseFrame MACRO			argsavename
;			release memory set up by createframe macro
;			restores RSP, and RBP to as-called values
;			after these instructions are executed, no LOCAL variables can be accessed
;			This needs to be done to restore the stack correctly, but can be done only once
;			Strongly suggest doing this just prior to return instruction, and that there is only one return instruction from the PROC
			MOV				RBP, argsavename
			LEAVE										; restore stack pointer (eliminating LOCAL storage), restore base pointer for caller
			ENDM


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