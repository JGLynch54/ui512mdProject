#pragma once

#ifndef ui512md_h
#define ui512md_h

//		ui51md.h
// 
//		File:			ui51md.h
//		Author:			John G.Lynch
//		Legal:			Copyright @2024, per MIT License below
//		Date:			June 19, 2024
//

#include "CommonTypeDefs.h"

extern "C"
{
	//			signatures ( from ui512md.asm )

	//	EXTERNDEF	mult_uT64 : PROC
	//	mult_uT64	multiply 512 bit multiplicand by 64 bit multiplier, giving 512 product, 64 bit overflow
	//	Prototype:	void mult_uT64 ( u64 * product, u64 * overflow, u64 * multiplicand, u64 multiplier );
	void mult_uT64 ( u64*, u64*, u64*, u64 );

	//	EXTERNDEF	mult_u : PROC
	//	mult_u		multiply 512 multiplicand by 512 multiplier, giving 512 product, overflow
	//	Prototype:	void mult_u ( u64 * product, u64 * overflow, u64 * multiplicand, u64 * multiplier );
	void mult_u ( u64*, u64*, u64*, u64* );

	//	EXTERNDEF	div_uT64 : PROC
	//	div_uT64	divide 512 bit dividend by 64 bit divisor, giving 512 bit quotient and 64 bit remainder
	//	Prototype:	s16 div_uT64 ( u64 * quotient, u64 * remainder, u64 * dividend, u64 divisor );
	s16 div_uT64 ( u64*, u64*, u64*, u64 );

	//	EXTERNDEF	div_u : PROC
	//	div_u		divide 512 bit dividend by 512 bit divisor, giving 512 bit quotient and remainder
	//	Prototype:	s16 div_u ( u64 * quotient, u64 * remainder, u64 * dividend, u64 * divisor );
	s16 div_u ( u64*, u64*, u64*, u64* );
}

#endif