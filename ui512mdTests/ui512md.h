#pragma once

#ifndef ui512md_h
#define ui512md_h

#include "CommonTypeDefs.h"

extern "C"
{
	//			signatures ( from ui512md.asm )

	//	EXTERNDEF	mult_u : PROC;
	// 	void mult_u ( u64* product, u64* overflow, u64* multiplicand, u64* multiplier );
	void mult_u ( u64*, u64*, u64*, u64* );

	//	EXTERNDEF	mult_uT64 : PROC;
	// 	void mult_uT64 ( u64* product, u64 overflow, u64* multiplicand, u64 multiplier );
	void mult_uT64 ( u64*, u64*, u64*, u64* );

	//	EXTERNDEF	div_u : PROC;
	// 	s16 div_u ( u64* quotient, u64* remainder, u64* dividend, u64* divisor );
	s16 div_u ( u64*, u64*, u64*, u64* );

	//	EXTERNDEF	div_uT64 : PROC;
	// 	s16 div_uT64 ( u64* quotient, u64 remainder, u64* dividend, u64 divisor );
	s16 div_uT64 ( u64*, u64, u64* u64 );
}

#endif