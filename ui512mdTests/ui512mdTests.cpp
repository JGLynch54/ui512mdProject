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
			u32 dist [ dec ] { 0 };
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
			// Note: the ui512a module must pass testing before these tests, as zero, add, and set are used in this test
			// Note: the ui512b module must pass testing before these tests, as 'or' and shifts are used in this test

			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0 };
			alignas ( 64 ) u64 num2 [ 8 ] { 0 };
			alignas ( 64 ) u64 num3 [ 8 ] { 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0 };
			alignas ( 64 ) u64 overflow [ 8 ] { 0 };
			alignas ( 64 ) u64 intermediateprod [ 8 ] { 0 };
			alignas ( 64 ) u64 intermediateovrf [ 8 ] { 0 };
			alignas ( 64 ) u64 expectedproduct [ 8 ] { 0 };
			alignas ( 64 ) u64 expectedoverflow [ 8 ] { 0 };

			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left, expected overflow is a shift right
			for ( int i = 0; i < runcount; i++ )
			{
				//	random initialize mutiplicand
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
				};

				//	initialize multiplier
				set_uT64 ( num2, 2ull );

				// calculate expected product
				shl_u ( expectedproduct, num1, u16 ( 1 ) );

				// calculate expected overflow
				shr_u ( expectedoverflow, num1, u16 ( 511 ) );

				// execute function under test (multiply)
				mult_u ( product, overflow, num1, num2 );

				//	check actual vs. expected
				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ],
						MSG ( L"Product at " << j << " failed " << i ) );
					Assert::AreEqual ( expectedoverflow [ j ], overflow [ j ],
						MSG ( L"Overflow at " << j << " failed " << i ) );
				}
			};

			string runmsg1 = "Multiply function testing. Simple multiply by 2 "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg1.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

			// Second test, a simple multiply by a random power of two. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for ( int i = 0; i < runcount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
				};

				set_uT64 ( num2, 1ull );

				u16 nrShift = RandomU64 ( &seed ) % 512;
				shl_u ( num2, num2, nrShift );

				shl_u ( expectedproduct, num1, nrShift );

				shr_u ( expectedoverflow, num1, ( 512 - nrShift ) );

				mult_u ( product, overflow, num1, num2 );

				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ],
						MSG ( L"Product at " << j << " failed " << i ) );
					Assert::AreEqual ( expectedoverflow [ j ], overflow [ j ],
						MSG ( L"Overflow at " << j << " failed " << i ) );
				}
			};

			string runmsg2 = "Multiply function testing. Multiply by random power of 2 "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg2.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

			// Third test, a multiply by sums of random powers of two. 
			// Building "expected" is a bit more complicated

			const u16 nrBits = 192;				// the generated random number will have nrBits randomly selected and "on"
			u16 BitsUsed [ nrBits ] = { 0 };	// intialize all to zero
			for ( int i = 0; i < runcount; i++ )
			{
				u16 bitcnt = 0;
				fill_n ( BitsUsed, nrBits, 0 );

				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
				};

				zero_u ( num2 );
				zero_u ( expectedproduct );
				zero_u ( expectedoverflow );

				// build multiplier as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result and overflow
				u16 nrShift = 0;
				for ( int j = 0; j < nrBits; j++ )
				{
					zero_u ( intermediateprod );
					zero_u ( intermediateovrf );

					// Find a bit (0 to 511) that hasn't already been selected in this random number
					bool nubit = false;
					nrShift = RandomU64 ( &seed ) % 512;
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
						// exited compare loop. Either exhausted list, or found match. Which?
						if ( nrShift == BitsUsed [ j2 ] )
						{
							nrShift = RandomU64 ( &seed ) % 512; // if match (already used), gen new, try again
						}
						else
						{
							nubit = true;						// else signal end (found a new bit to use) 
							BitsUsed [ bitcnt++ ] = nrShift;	// and save it so it won't be used again in this iteration
						};
					};

					// use selected bit to build a random number (through shift/or)
					// then build expected result of multiply (through shift / add)

					// Multiplier:
					set_uT64 ( num3, 1ull );
					shl_u ( num3, num3, nrShift );
					or_u ( num2, num2, num3 );

					// Expected:
					shl_u ( intermediateprod, num1, nrShift );
					s32 carry = add_u ( expectedproduct, expectedproduct, intermediateprod );
					shr_u ( intermediateovrf, num1, 512 - nrShift );
					add_u ( expectedoverflow, expectedoverflow, intermediateovrf );
					if ( carry != 0 )
					{
						add_uT64 ( expectedoverflow, expectedoverflow, 1ull );
					};
				};

				// Got a random multiplicand and multiplier. Execute function under test
				mult_u ( product, overflow, num1, num2 );

				// Compare results to expected (aborts test if they don't match)
				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ],
						MSG ( L"Product at " << j << " failed " << i ) );
					Assert::AreEqual ( expectedoverflow [ j ], overflow [ j ],
						MSG ( L"Overflow at " << j << " failed " << i ) );
				}
			};

			string runmsg3 = "Multiply function testing. Multiply by sums of random powers of two "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg3.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512md_01_mul_timing )
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0 };
			alignas ( 64 ) u64 num2 [ 8 ] { 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0 };
			alignas ( 64 ) u64 overflow [ 8 ] { 0 };

			for ( int i = 0; i < 8; i++ )
			{
				num1 [ i ] = RandomU64 ( &seed );
				num2 [ i ] = RandomU64 ( &seed );
			}

			for ( int i = 0; i < timingcount; i++ )
			{
				mult_u ( product, overflow, num1, num2 );
			};

			string runmsg = "Multiply function timing. Ran " +
				to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};

		TEST_METHOD ( ui512md_02_mul64 )
		{
			// mult_u tests
			// multistage testing, part for use as debugging, progressively "real" testing
			// Note: the ui512a module must pass testing before these tests, as adds are used in this test
			// Note: the ui512b module must pass testing before these tests, as 'or' and shifts are used in this test

			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0 };
			alignas ( 64 ) u64 intermediateprod [ 8 ] { 0 };
			alignas ( 64 ) u64 expectedproduct [ 8 ] { 0 };

			u64 num2 = 0;
			u64 num3 = 0;
			u64 overflow = 0;
			u64 intermediateovrf = 0;
			u64 expectedoverflow = 0;

			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left,
			// and expected overflow is a shift right
			for ( int i = 0; i < runcount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
				};

				num2 = 2;
				shl_u ( expectedproduct, num1, u16 ( 1 ) );
				expectedoverflow = num1 [ 0 ] >> 63;

				mult_uT64 ( product, &overflow, num1, num2 );

				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ],
						MSG ( L"Product at " << j << " failed " << i ) );
				};

				Assert::AreEqual ( expectedoverflow, overflow, MSG ( L"Overflow failed " << i ) );
			};

			string runmsg1 = "Multiply (u64) function testing. Simple multiply by 2 " +
				to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg1.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

			// Second test, a simple multiply by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift left,
			// and expected overflow is a shift right
			for ( u16 nrShift = 0; nrShift < 64; nrShift++ )	// rather than a random bit, cycle thru all 64 bits 
			{
				for ( int i = 0; i < runcount / 64; i++ )
				{
					for ( int j = 0; j < 8; j++ )
					{
						num1 [ j ] = RandomU64 ( &seed );
					};

					num2 = 1ull << nrShift;
					shl_u ( expectedproduct, num1, nrShift );
					expectedoverflow = ( nrShift == 0 ) ? 0 : num1 [ 0 ] >> ( 64 - nrShift );

					mult_uT64 ( product, &overflow, num1, num2 );

					for ( int j = 0; j < 8; j++ )
					{
						Assert::AreEqual ( expectedproduct [ j ], product [ j ],
							MSG ( L"Product at " << j << " failed " << nrShift << i ) );
					}
					Assert::AreEqual ( expectedoverflow, overflow, MSG ( L"Overflow failed " << nrShift << " at " << i ) );
				};
			}

			string runmsg2 = "Multiply (u64) function testing. Multiply by sequential powers of 2 "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg2.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

			// Third test, a multiply by random sums of powers of two. 
			// Building "expected" is a bit more complicated
			const u16 nrBits = 24;
			u16 BitsUsed [ nrBits ] = { 0 };
			for ( int i = 0; i < runcount; i++ )
			{
				u16 bitcnt = 0;
				fill_n ( BitsUsed, nrBits, 0 );

				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
					product [ j ] = 0;
					expectedproduct [ j ] = 0;
				};

				expectedoverflow = 0;
				overflow = 0;
				num2 = 0;
				num3 = 0;

				// build multiplier as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result

				u16 nrShift = 0;
				for ( int j = 0; j < nrBits; j++ )
				{
					bool nubit = false;
					zero_u ( intermediateprod );
					intermediateovrf = 0;

					// Find a bit (0 -> 63) that hasn't already been used in this random number
					nrShift = RandomU64 ( &seed ) % 64;
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
							nrShift = RandomU64 ( &seed ) % 64;
						}
						else
						{
							nubit = true;
							BitsUsed [ bitcnt++ ] = nrShift;
						};
					};

					// use selected bit to build a random number (through shift/add)
					// then build / project expected result of multiply (also through shift / add)

					// Multiplier:
					num3 = 1ull << nrShift;
					num2 += num3;

					// Expected:
					shl_u ( intermediateprod, num1, nrShift );
					s32 carry = add_u ( expectedproduct, expectedproduct, intermediateprod );
					intermediateovrf = ( nrShift == 0 ) ? 0 : num1 [ 0 ] >> ( 64 - nrShift );
					expectedoverflow += intermediateovrf;
					if ( carry != 0 )
					{
						expectedoverflow++;
					};
				};

				mult_uT64 ( product, &overflow, num1, num2 );

				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedproduct [ j ], product [ j ],
						MSG ( L"Product at " << j << " failed " << i << " num2: " << num2 ) );
				}
				Assert::AreEqual ( expectedoverflow, overflow, MSG ( L"Overflow failed " << i ) );
			};

			string runmsg3 = "Multiply (u64) function testing. Multiply by random number "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg3.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512md_02_mul64_timing )
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0 };
			u64 num2 = 0;
			u64 overflow = 0;

			for ( int j = 0; j < 8; j++ )
			{
				num1 [ j ] = RandomU64 ( &seed );
			};

			num2 = RandomU64 ( &seed );

			for ( int i = 0; i < timingcount; i++ )
			{
				mult_uT64 ( product, &overflow, num1, num2 );
			};

			string runmsg = "Multiply (u64) function timing. Ran " + to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};

		TEST_METHOD ( ui512md_03_div )
		{
			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 quotient [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 remainder [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			int adjruncount = runcount / 64;
			for ( int i = 0; i < adjruncount; i++ )
			{
				for ( int m = 7; m >= 0; m-- )
				{
					for ( int j = 7; j >= 0; j-- )
					{
						for ( int l = 0; l < 8; l++ )
						{
							num1 [ l ] = RandomU64 ( &seed );
							num2 [ l ] = 0;
							quotient [ l ] = 0;
							remainder [ l ] = 0;
						};
						num2 [ m ] = 1;
						;
						div_u ( quotient, remainder, num1, num2 );


						for ( int v = 7; v >= 0; v-- )
						{
							int qidx, ridx = 0;
							u64 qresult, rresult = 0;

							qidx = v - ( 7 - m );
							qresult = ( qidx >= 0 ) ? qresult = ( v >= ( 7 - m ) ) ? num1 [ qidx ] : 0ull : qresult = 0;
							rresult = ( v > m ) ? num1 [ v ] : 0ull;

							Assert::AreEqual ( quotient [ v ], qresult, L"Quotient incorrect" );
							Assert::AreEqual ( remainder [ v ], rresult, L" Remainder incorrect" );
						};

						num2 [ m ] = 0;
					};
				};
			};
			string runmsg = "Divide function testing. Ran tests "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512md_03_div_timing )
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas ( 64 ) u64 dividend [ 8 ] { 0 };
			alignas ( 64 ) u64 quotient [ 8 ] { 0 };
			alignas ( 64 ) u64 divisor [ 8 ] { 0 };
			alignas ( 64 ) u64 remainder [ 8 ] { 0 };

			for ( int i = 0; i < 8; i++ )
			{
				dividend [ i ] = RandomU64 ( &seed );
				divisor [ i ] = RandomU64 ( &seed );
			}
			zero_u ( quotient );
			zero_u ( remainder );

			for ( int i = 0; i < timingcount; i++ )
			{
				div_u ( quotient, remainder, dividend, divisor );
			};

			string runmsg = "Divide function timing. Ran "
				+ to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};
		TEST_METHOD ( ui512md_04_div64 )
		{
			u64 seed = 0;
			alignas ( 64 ) u64 dividend [ 8 ] { 0 };
			alignas ( 64 ) u64 quotient [ 8 ] { 0 };
			alignas ( 64 ) u64 expectedquotient [ 8 ] { 0 };
			u64 divisor = 0;
			u64 remainder = 0;
			u64 expectedremainder = 0;


			// First test, a simple divide by two. 
			// Easy to check as the expected answer is a shift right,
			// and expected remainder is a shift left

			for ( int i = 0; i < runcount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					dividend [ j ] = RandomU64 ( &seed );
				};
				zero_u ( quotient );

				divisor = 2;
				shr_u ( expectedquotient, dividend, u16 ( 1 ) );
				expectedremainder = ( dividend [ 7 ] << 63 ) >> 63;

				div_uT64 ( quotient, &remainder, dividend, divisor );

				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedquotient [ j ], quotient [ j ],
						MSG ( L"Quotient at " << j << " failed " << i ) );
				};

				Assert::AreEqual ( expectedremainder, remainder, MSG ( L"Remainder failed " << i ) );
			};

			string runmsg1 = "Divide (u64) function testing. Simple divide by 2 " +
				to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg1.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );


			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for ( u16 nrShift = 0; nrShift < 64; nrShift++ )	// rather than a random bit, cycle thru all 64 bits 
			{
				for ( int i = 0; i < runcount / 64; i++ )
				{
					for ( int j = 0; j < 8; j++ )
					{
						dividend [ j ] = RandomU64 ( &seed );
					};

					divisor = 1ull << nrShift;
					shr_u ( expectedquotient, dividend, nrShift );
					expectedremainder = ( nrShift == 0 ) ? 0 : ( dividend [ 7 ] << ( 64 - nrShift ) ) >> ( 64 - nrShift );

					div_uT64 ( quotient, &remainder, dividend, divisor );

					for ( int j = 0; j < 8; j++ )
					{
						Assert::AreEqual ( expectedquotient [ j ], quotient [ j ],
							MSG ( L"Quotient at " << j << " failed " << nrShift << i ) );
					}
					Assert::AreEqual ( expectedremainder, remainder, MSG ( L"Remainder failed " << nrShift << " at " << i ) );
				};
			}

			string runmsg2 = "Divide (u64) function testing. Divide by sequential powers of 2 "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg2.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );


			// Third test, a divide by random sums of powers of two. 
			// Building "expected" is a bit more complicated

			alignas ( 64 )  u64 intermediatediv [ 8 ] { 0 };
			u64 intermediaterem = 0;
			const u16 nrBits = 24;
			u16 BitsUsed [ nrBits ] = { 0 };
			for ( int i = 0; i < runcount; i++ )
			{
				u16 bitcnt = 0;
				fill_n ( BitsUsed, nrBits, 0 );

				for ( int j = 0; j < 8; j++ )
				{
					dividend [ j ] = RandomU64 ( &seed );
					quotient [ j ] = 0;
					expectedquotient [ j ] = 0;
				};

				expectedremainder = 0;
				remainder = 0;
				u64 num2 = 0;
				u64 num3 = 0;

				// build divisor as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result

				u16 nrShift = 0;
				for ( int j = 0; j < nrBits; j++ )
				{
					bool nubit = false;
					zero_u ( intermediatediv );
					intermediaterem = 0;

					// Find a bit (0 -> 63) that hasn't already been used in this random number
					nrShift = RandomU64 ( &seed ) % 64;
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
							nrShift = RandomU64 ( &seed ) % 64;
						}
						else
						{
							nubit = true;
							BitsUsed [ bitcnt++ ] = nrShift;
						};
					};

					// use selected bit to build a random number (through shift/add)
					// then build / project expected result of divide (also through shift / add)

					// Divisor:
					num3 = 1ull << nrShift;
					num2 += num3;

					// Expected:
					shl_u ( intermediatediv, dividend, nrShift );
					s32 carry = add_u ( expectedquotient, expectedquotient, intermediatediv );
					intermediaterem = ( nrShift == 0 ) ? 0 : dividend [ 0 ] >> ( 64 - nrShift );
					expectedremainder += intermediaterem;
					if ( carry != 0 )
					{
						expectedremainder++;
					};
				};

				div_uT64 ( quotient, &remainder, dividend, divisor );

				for ( int j = 0; j < 8; j++ )
				{
					Assert::AreEqual ( expectedquotient [ j ], quotient [ j ],
						MSG ( L"Product at " << j << " failed " << i << " num2: " << num2 ) );
				}
				Assert::AreEqual ( expectedremainder, remainder, MSG ( L"Overflow failed " << i ) );
			};

			string runmsg3 = "Divide (u64) function testing. Divide by random number "
				+ to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg3.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512md_04_div64_timing )
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas ( 64 ) u64 dividend [ 8 ] { 0 };
			alignas ( 64 ) u64 quotient [ 8 ] { 0 };
			u64 divisor = 0;
			u64 remainder = 0;

			for ( int i = 0; i < 8; i++ )
			{
				dividend [ i ] = RandomU64 ( &seed );
			}
			zero_u ( quotient );
			divisor = RandomU64 ( &seed );

			for ( int i = 0; i < timingcount; i++ )
			{
				div_uT64 ( quotient, &remainder, dividend, divisor );
			};

			string runmsg = "Divide by u64  function timing. Ran "
				+ to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};
	};
};
