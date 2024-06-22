//		ui51mdTests
// 
//		File:			ui512mdTests.cpp
//		Author:			John G.Lynch
//		Legal:			Copyright @2024, per MIT License below
//		Date:			June 19, 2024
//
//		ui512 is a small project to provide basic operations for a variable type of unsigned 512 bit integer.
//		The basic operations : zero, copy, compare, add, subtract.
//		Other optional modules provide bit ops and multiply / divide.
//		It is written in assembly language, using the MASM ( ml64 ) assembler provided as an option within Visual Studio.
//		( currently using VS Community 2022 17.9.6 )
//		It provides external signatures that allow linkage to C and C++ programs,
//		where a shell / wrapper could encapsulate the methods as part of an object.
//		It has assembly time options directing the use of Intel processor extensions : AVX4, AVX2, SIMD, or none :
//		( Z ( 512 ), Y ( 256 ), or X ( 128 ) registers, or regular Q ( 64bit ) ).
//		If processor extensions are used, the caller must align the variables declared and passed
//		on the appropriate byte boundary ( e.g. alignas 64 for 512 )
//		This module is very light - weight ( less than 1K bytes ) and relatively fast,
//		but is not intended for all processor types or all environments.
//		Use for private ( hobbyist ), or instructional,
//		or as an example for more ambitious projects is all it is meant to be.
// 
// 		ui512b provides basic bit-oriented operations: shift left, shift right, and, or, not,
//		least significant bit and most significant bit.
//
//		This module, ui512md, adds mmultiply and divide funnctions.
//
//		This sub - project: ui512mdTests, is a unit test project that invokes each of the routines in the ui512md assembly.
//		It runs each assembler proc with pseudo - random values.
//		It validates ( asserts ) expected and returned results.
//		It also runs each repeatedly for comparative timings.
//		It provides a means to invoke and debug.
//		It illustrates calling the routines from C++.

#include "pch.h"
#include "CppUnitTest.h"

#include <format>

#include "ui512a.h"
#include "ui512b.h"
#include "ui512md.h"

using namespace std;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Macro helper to construct and pass message from with Assert
#define MSG(msg) [&]{ std::wstringstream _s; _s << msg; return _s.str(); }().c_str()

namespace ui512mdTests
{
	TEST_CLASS ( ui512mdTests )
	{
	public:

		const s32 runcount = 1000;
		const s32 timingcount = 1000000;

		/// <summary>
		/// Random number generator
		/// uses linear congruential method 
		/// ref: Knuth, Art Of Computer Programming, Vol. 2, Seminumerical Algorithms, 3rd Ed. Sec 3.2.1
		/// </summary>
		/// <param name="seed">if zero, will supply with: 4294967291</param>
		/// <returns>Pseudo-random number from zero to ~2^63 (9223372036854775807)</returns>
		u64 RandomU64 ( u64* seed )
		{
			const u64 m = 9223372036854775807ull;			// 2^63 - 1, a Mersenne prime
			const u64 a = 68719476721ull;					// closest prime below 2^36
			const u64 c = 268435399ull;						// closest prime below 2^28
			// suggested seed: around 2^32, 4294967291
			*seed = ( *seed == 0ull ) ? ( a * 4294967291ull + c ) % m : ( a * *seed + c ) % m;
			return *seed;
		};

		TEST_METHOD ( random_number_generator )
		{
			//	Check distibution of "random" numbers
			u64 seed = 0;
			const u32 dec = 10;
			u32 dist [ dec ] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			const u64 split = 9223372036854775807ull / dec;
			u32 distc = 0;
			float deviation = 0.0;
			float varience = 0.0;
			const u32 randomcount = 1000000;
			const s32 norm = randomcount / dec;

			for ( u32 i = 0; i < randomcount; i++ )
			{
				seed = RandomU64 ( &seed );
				dist [ u64 ( seed / split ) ]++;
			};

			string msgd = "Evaluation of pseudo-random number generator.\n\n";
			msgd += format ( "Generated {0:*>8} numbers.\n", randomcount );
			msgd += format ( "Counted occurances of those numbers by decile, each decile {0:*>20}.\n", split );
			msgd += format ( "Distribution of numbers accross the deciles indicates the quality of the generator.\n\n" );
			msgd += "Distribution by decile:";
			string msgv = "Variance from mean:\t";

			for ( int i = 0; i < 10; i++ )
			{
				deviation = float ( abs ( long ( norm ) - long ( dist [ i ] ) ) );
				varience = float ( deviation ) / float ( norm ) * 100.0f;
				msgd += format ( "\t{:6d}", dist [ i ] );
				msgv += format ( "\t{:5.3f}% ", varience );
				distc += dist [ i ];
			};

			msgd += "\t\tDecile counts sum to: " + to_string ( distc ) + "\n";
			Logger::WriteMessage ( msgd.c_str ( ) );
			msgv += '\n';
			Logger::WriteMessage ( msgv.c_str ( ) );
		};

		TEST_METHOD ( ui512md_01_mul )
		{
			// mult_u tests
			// multistage testing, part for use as debugging, progressively "real" testing
			// Note: the ui512a module must pass testing before these tests, as adds are used in this test
			// Note: the ui512b module must pass testing before these tests, as shifts are used in this test
			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 num3 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 overflow [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 intermediateprod [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 intermediateovrf [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 expectedproduct [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 expectedoverflow [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };

			// First test, a simple multiply by two. 
			// Easy to check as the answer is a shift left, overflow is a shift right
			for ( int i = 0; i < runcount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
					num2 [ j ] = 0;
					product [ j ] = 0;
					overflow [ j ] = 0;
					expectedproduct [ j ] = 0;
					expectedoverflow [ j ] = 0;
				};

				num2 [ 7 ] = 2;
				shl_u ( expectedproduct, num1, u16 ( 1 ) );
				if ( expectedproduct [ 7 ] == 0ull )
				{
					break;
				};
				Assert::AreNotEqual ( 0ull, expectedproduct [ 7 ] );
				shr_u ( expectedoverflow, num1, u16 ( 511 ) );
				mult_u ( product, overflow, num1, num2 );
				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ], MSG ( L"Product at " << j << " failed " << i ) );
					Assert::AreEqual ( expectedoverflow [ j ], overflow [ j ], MSG ( L"Overflow at " << j << " failed " << i ) );
				}
			};

			string runmsg1 = "Multiply function testing. Simple multiply by 2 " + to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg1.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

			// Second test, a simple multiply by a random power of two. 
			// Still relatively easy to check as answer is a shift left, overflow is a shift right
			for ( int i = 0; i < runcount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
					num2 [ j ] = 0;
					product [ j ] = 0;
					overflow [ j ] = 0;
					expectedproduct [ j ] = 0;
					expectedoverflow [ j ] = 0;
				};

				num2 [ 7 ] = 1;
				u16 nrShift = RandomU64 ( &seed ) % 512 - 1;
				shl_u ( num2, num2, nrShift );
				shl_u ( expectedproduct, num1, nrShift );
				shr_u ( expectedoverflow, num1, 512 - nrShift );
				mult_u ( product, overflow, num1, num2 );
				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ], MSG ( L"Product at " << j << " failed " << i ) );
					Assert::AreEqual ( expectedoverflow [ j ], overflow [ j ], MSG ( L"Overflow at " << j << " failed " << i ) );
				}
			};

			string runmsg2 = "Multiply function testing. Multiply by random power of 2 " + to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg2.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

			// Third test, a multiply by random sums of powers of two. 
			// Building "expected" is a bit more complicated
			for ( int i = 0; i < runcount; i++ )
			{
				const u16 nrBits = 128;
				u16 bitcnt = 0;
				bool nubit = false;
				u16 BitsUsed [ nrBits ] = { 0 }; // intialize all to zero
				fill_n ( BitsUsed, nrBits, 0 );
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
					num2 [ j ] = 0;
					num3 [ j ] = 0;
					product [ j ] = 0;
					overflow [ j ] = 0;
					intermediateprod [ j ] = 0;
					intermediateovrf [ j ] = 0;
					expectedproduct [ j ] = 0;
					expectedoverflow [ j ] = 0;
				};

				// build multiplier as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result
				for ( int j = 0; j < nrBits; j++ )
				{
					for ( int j3 = 0; j3 < 8; j3++ )
					{
						intermediateprod [ j3 ] = 0;
						intermediateovrf [ j3 ] = 0;
						num3 [ j3 ] = 0;
					};

					// Find a bit (0 -> 512) that hasn't already been used in this random number
					u16 nrShift = RandomU64 ( &seed ) % 512 - 1;
					u16 k = 0;
					while ( !nubit )
					{
						u16 j2 = 0;
						for ( ; j2 < bitcnt; j2++ )
						{
							if ( nrShift == BitsUsed [ j2 ] )
							{
								break;
							}
						};
						if ( nrShift == BitsUsed [ j2 ] )
						{
							nrShift = RandomU64 ( &seed ) % 512 - 1;
						}
						else
						{
							nubit = true;
							BitsUsed [ bitcnt++ ] = nrShift;
						};
					};

					// use selected bit to build a random number (through shift/add)
					// then build / project expected result of multiply (also through shift / add)
					num3 [ 7 ] = 1;
					// Multiplier:
					shl_u ( num3, num3, nrShift );
					add_u ( num2, num2, num3 );
					// Expected:
					shl_u ( intermediateprod, num1, nrShift );
					s32 carry = add_u ( expectedproduct, expectedproduct, intermediateprod );
					shr_u ( intermediateovrf, num1, 512 - nrShift );
					add_u ( expectedoverflow, expectedoverflow, intermediateovrf );
					if ( carry != 0 )
					{
						u64 addone = 1;
						add_uT64 ( expectedoverflow, expectedoverflow, addone );
					};
				};

				mult_u ( product, overflow, num1, num2 );
				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ], MSG ( L"Product at " << j << " failed " << i ) );
					Assert::AreEqual ( expectedoverflow [ j ], overflow [ j ], MSG ( L"Overflow at " << j << " failed " << i ) );
				}
			};

			string runmsg3 = "Multiply function testing. Multiply by random number " + to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg3.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512md_01_mul_timing )
		{
			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 overflow [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };

			for ( int i = 0; i < 8; i++ )
			{
				num1 [ i ] = RandomU64 ( &seed );
				num2 [ i ] = RandomU64 ( &seed );
				product [ i ] = 0;
				overflow [ i ] = 0;
			}

			for ( int i = 0; i < timingcount; i++ )
			{
				mult_u ( product, overflow, num1, num2 );
			};

			string runmsg = "Multiply function timing. Ran " + to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};
		//
		//		TEST_METHOD ( ui512md_02_mul64 )
		//		{
		//			u64 seed = 0;
		//			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			u64 num2;
		//			u64 overflow;
		//			for ( int i = 0; i < runcount; i++ )
		//			{
		//				for ( int j = 0; j < 8; j++ )
		//				{
		//					num1 [ j ] = RandomU64 ( &seed );
		//					product [ j ] = 0;
		//				};
		//				overflow = 0;
		//				num2 = RandomU64 ( &seed );
		//				;
		//				mult_uT64 ( product, &overflow, num1, num2 );
		//
		//
		//			};
		//			string runmsg = "Multiply function testing. Ran tests " + to_string ( runcount ) + " times, each with pseudo random values.\n";;
		//			Logger::WriteMessage ( runmsg.c_str ( ) );
		//			//	Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		//
		//		};
		//
		TEST_METHOD ( ui512md_02_mul64_timing )
		{
			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			u64 num2;
			u64 overflow;

			for ( int j = 0; j < 8; j++ )
			{
				num1 [ j ] = RandomU64 ( &seed );
				product [ j ] = 0;
			};
			overflow = 0;
			num2 = RandomU64 ( &seed );

			for ( int i = 0; i < timingcount; i++ )
			{
				mult_uT64 ( product, &overflow, num1, num2 );
			};

			string runmsg = "Multiply function timing. Ran " + to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};
		// 
		//		TEST_METHOD ( ui512md_03_div )
		//		{
		//			u64 seed = 0;
		//			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 quotient [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 remainder [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			int adjruncount = runcount / 64;
		//			for ( int i = 0; i < adjruncount; i++ )
		//			{
		//				for ( int m = 7; m >= 0; m-- )
		//				{
		//					for ( int j = 7; j >= 0; j-- )
		//					{
		//						for ( int l = 0; l < 8; l++ )
		//						{
		//							num1 [ l ] = RandomU64 ( &seed );
		//							num2 [ l ] = 0;
		//							quotient [ l ] = 0;
		//							remainder [ l ] = 0;
		//						};
		//						num2 [ m ] = 1;
		//						;
		//						div_u ( quotient, remainder, num1, num2 );
		//
		//
		//						for ( int v = 7; v >= 0; v-- )
		//						{
		//							int qidx, ridx = 0;
		//							u64 qresult, rresult = 0;
		//
		//							qidx = v - ( 7 - m );
		//							qresult = ( qidx >= 0 ) ? qresult = ( v >= ( 7 - m ) ) ? num1 [ qidx ] : 0ull : qresult = 0;
		//							rresult = ( v > m ) ? num1 [ v ] : 0ull;
		//
		//							//if ( quotient [ v ] != qresult )
		//							//{
		//							//	break;
		//							//};
		//							Assert::AreEqual ( quotient [ v ], qresult, L"Quotient incorrect" );
		//							Assert::AreEqual ( remainder [ v ], rresult, L" Remainder incorrect" );
		//						};
		//
		//						num2 [ m ] = 0;
		//					};
		//				};
		//			};
		//			string runmsg = "Divide function testing. Ran tests " + to_string ( adjruncount * 64 ) + " times, each with pseudo random values.\n";;
		//			Logger::WriteMessage ( runmsg.c_str ( ) );
		//			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		//		};
		//
		//		TEST_METHOD ( ui512md_03_div_timing )
		//		{
		//			u64 seed = 0;
		//			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 overflow [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//
		//			for ( int i = 0; i < 8; i++ )
		//			{
		//				num1 [ i ] = RandomU64 ( &seed );
		//				num2 [ i ] = RandomU64 ( &seed );
		//				product [ i ] = 0;
		//				overflow [ i ] = 0;
		//			}
		//
		//			for ( int i = 0; i < timingcount; i++ )
		//			{
		//				div_u ( product, overflow, num1, num2 );
		//			};
		//
		//			string runmsg = "Divide function timing. Ran " + to_string ( timingcount ) + " times.\n";
		//			Logger::WriteMessage ( runmsg.c_str ( ) );
		//		};
		//		TEST_METHOD ( ui512md_04_div64 )
		//		{
		//
		//		};
		//
		//		TEST_METHOD ( ui512md_04_div64_timing )
		//		{
		//			u64 seed = 0;
		//			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//			alignas ( 64 ) u64 overflow [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
		//
		//			for ( int i = 0; i < 8; i++ )
		//			{
		//				num1 [ i ] = RandomU64 ( &seed );
		//				num2 [ i ] = RandomU64 ( &seed );
		//				product [ i ] = 0;
		//				overflow [ i ] = 0;
		//			}
		//
		//			for ( int i = 0; i < timingcount; i++ )
		//			{
		//				mult_u ( product, overflow, num1, num2 );
		//			};
		//
		//			string runmsg = "Multiply function timing. Ran " + to_string ( timingcount ) + " times.\n";
		//			Logger::WriteMessage ( runmsg.c_str ( ) );
		//		};
	};
};
