//		ui51mdTests
// 
//		File:			ui512mdTests.cpp
//		Author:			John G.Lynch
//		Legal:			Copyright @2024, per MIT License below
//		Date:			June 19, 2024
//
//		ui512 is a small project to provide basic operations for a variable type of unsigned 512 bit integer.
//
//		ui512a provides basic operations : zero, copy, compare, add, subtract.
//		ui512b provides basic bit - oriented operations : shift left, shift right, and, or , not, least significant bit and most significant bit.
//      ui512md provides multiply and divide.
//
//		It is written in assembly language, using the MASM(ml64) assembler provided as an option within Visual Studio.
//		(currently using VS Community 2022 17.9.6)
//
//		It provides external signatures that allow linkage to C and C++ programs,
//		where a shell / wrapper could encapsulate the methods as part of an object.
//
// 		It has assembly time options directing the use of Intel processor extensions : AVX4, AVX2, SIMD, or none :
//		(Z(512), Y(256), or X(128) registers, or regular Q(64bit)).
//
//		If processor extensions are used, the caller must align the variables declared and passed
//		on the appropriate byte boundary(e.g. alignas 64 for 512)
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
	TEST_CLASS(ui512mdTests)
	{
	public:

		const s32 runcount = 1000;
		const s32 timingcount = 10000000;

		/// <summary>
		/// Random number generator
		/// uses linear congruential method 
		/// ref: Knuth, Art Of Computer Programming, Vol. 2, Seminumerical Algorithms, 3rd Ed. Sec 3.2.1
		/// </summary>
		/// <param name="seed">if zero, will supply with: 4294967291</param>
		/// <returns>Pseudo-random number from zero to ~2^63 (9223372036854775807)</returns>
		u64 RandomU64(u64* seed)
		{
			const u64 m = 9223372036854775807ull;			// 2^63 - 1, a Mersenne prime
			const u64 a = 68719476721ull;					// closest prime below 2^36
			const u64 c = 268435399ull;						// closest prime below 2^28
			// suggested seed: around 2^32, 4294967291
			*seed = (*seed == 0ull) ? (a * 4294967291ull + c) % m : (a * *seed + c) % m;
			return *seed;
		};

		TEST_METHOD(random_number_generator)
		{
			//	Check distibution of "random" numbers
			u64 seed = 0;
			const u32 dec = 10;
			u32 dist[dec]{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

			const u64 split = 9223372036854775807ull / dec;
			u32 distc = 0;
			float varsum = 0.0;
			float deviation = 0.0;
			float D = 0.0;
			float sumD = 0.0;
			float varience = 0.0;
			const u32 randomcount = 1000000;
			const s32 norm = randomcount / dec;
			for (u32 i = 0; i < randomcount; i++)
			{
				seed = RandomU64(&seed);
				dist[u64(seed / split)]++;
			};

			string msgd = "Evaluation of pseudo-random number generator.\n\n";
			msgd += format("Generated {0:*>8} numbers.\n", randomcount);
			msgd += format("Counted occurrences of those numbers by decile, each decile {0:*>20}.\n", split);
			msgd += format("Distribution of numbers across the deciles indicates the quality of the generator.\n\n");
			msgd += "Distribution by decile:";
			string msgv = "Variance from mean:\t";
			string msgchi = "Variance ^2 (chi):\t";

			for (int i = 0; i < 10; i++)
			{
				deviation = float(abs(long(norm) - long(dist[i])));
				D = (deviation * deviation) / float(long(norm));
				sumD += D;
				varience = float(deviation) / float(norm) * 100.0f;
				varsum += varience;
				msgd += format("\t{:6d}", dist[i]);
				msgv += format("\t{:5.3f}% ", varience);
				msgchi += format("\t{:5.3f}% ", D);
				distc += dist[i];
			};

			msgd += "\t\tDecile counts sum to: " + to_string(distc) + "\n";
			Logger::WriteMessage(msgd.c_str());
			msgv += "\t\tVariance sums to: ";
			msgv += format("\t{:6.3f}% ", varsum);
			msgv += '\n';
			Logger::WriteMessage(msgv.c_str());
			msgchi += "\t\tChi distribution: ";
			msgchi += format("\t{:6.3f}% ", sumD);
			msgchi += '\n';
			Logger::WriteMessage(msgchi.c_str());
		};

		TEST_METHOD(ui512md_01_mul)
		{
			// mult_u tests
			// multistage testing, part for use as debugging, progressively "real" testing
			// Note: the ui512a module must pass testing before these tests, as zero, add, and set are used in this test
			// Note: the ui512b module must pass testing before these tests, as 'or' and shifts are used in this test

			u64 seed = 0;
			alignas (64) u64 num1[8]{ 0 };
			alignas (64) u64 num2[8]{ 0 };
			alignas (64) u64 num3[8]{ 0 };
			alignas (64) u64 product[8]{ 0 };
			alignas (64) u64 overflow[8]{ 0 };
			alignas (64) u64 intermediateprod[8]{ 0 };
			alignas (64) u64 intermediateovrf[8]{ 0 };
			alignas (64) u64 expectedproduct[8]{ 0 };
			alignas (64) u64 expectedoverflow[8]{ 0 };
			alignas (64) u64 work[8]{ 0 };

			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left, expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				//	random initialize multiplicand
				for (int j = 0; j < 8; j++)
				{
					num1[j] = RandomU64(&seed);
				};

				//	initialize multiplier
				set_uT64(num2, 2ull);

				// calculate expected product
				shl_u(expectedproduct, num1, u16(1));

				// calculate expected overflow
				shr_u(expectedoverflow, num1, u16(511));

				// execute function under test (multiply)
				mult_u(product, overflow, num1, num2);

				//	check actual vs. expected
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j],
						MSG(L"Overflow at " << j << " failed " << i));
				}
			};

			string runmsg1 = "Multiply function testing. First test, a simple multiply by two.  "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg1.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");

			// Second test, a simple multiply by a random power of two. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					num1[j] = RandomU64(&seed);
				};

				set_uT64(num2, 1ull);

				u16 nrShift = RandomU64(&seed) % 512;
				shl_u(num2, num2, nrShift);

				shl_u(expectedproduct, num1, nrShift);

				shr_u(expectedoverflow, num1, (512 - nrShift));

				mult_u(product, overflow, num1, num2);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j],
						MSG(L"Overflow at " << j << " failed " << i));
				}
			};

			string runmsg2 = "Multiply function testing. Second test, a simple multiply by a random power of two. "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg2.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");

			// Third test, a simple multiply by a random 64 bit integer. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					num1[j] = RandomU64(&seed);
				};

				u64 num2_64 = RandomU64(&seed);
				u64 expectedovfl_64 = 0;
				set_uT64(num2, num2_64);

				mult_uT64(expectedproduct, &expectedovfl_64, num1, num2_64);
				set_uT64(expectedoverflow, expectedovfl_64);

				mult_u(product, overflow, num1, num2);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j],
						MSG(L"Overflow at " << j << " failed " << i));
				}
			};

			string runmsg3 = "Multiply function testing. Third test, a multiply by a random 64 bit value. "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg3.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");


			// Fourth test, a multiply by sums of random powers of two into a 64 bit value. 
			// Building "expected" is a bit more complicated. This test is more about correctly buiding "expected"

			for (int i = 0; i < runcount; i++)
			{
				bool rbits[64];
				std::memset(rbits, 0, sizeof(rbits));

				for (int j = 0; j < 63; j++)
				{
					rbits[j] = (RandomU64(&seed) % 2) == 1;
				};

				for (int j = 7; j >= 0; j--)
				{
					num1[j] = RandomU64(&seed);
				};

				// build multiplier as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result and overflow

				zero_u(num2);
				zero_u(expectedproduct);
				zero_u(expectedoverflow);

				u16 nrBits = (RandomU64(&seed) % 36) + 2; //  Nr Bits somewhere between 2 and 36? 
				for (int j = 0; j < nrBits; j++)
				{
					zero_u(intermediateprod);
					zero_u(intermediateovrf);
					// use selected bit to build a random number (through shift/or)
					// then build expected result of multiply (through shift / add)
					bool found = false;
					u16 nrShift = 0;
					while ((nrShift == 0) && !found)
					{
						u16 rBit = RandomU64(&seed) % 64;
						for (u16 idx = rBit; idx < 64; idx++)
						{
							u16 bt = idx % 64;
							u64 msk = 1ull << bt;
							if (rbits[idx] && !(num2[7] & msk))
							{
								nrShift = idx;
								found = true;
								break;
							}
						};
						if (!found)
						{
							break;
						};
					};

					if (found)
					{
						// Multiplier:
						set_uT64(num3, 1ull);
						shl_u(num3, num3, nrShift);
						or_u(num2, num2, num3);

						// Calculate expected product and overflow:
						shl_u(intermediateprod, num1, nrShift);
						s16 prad = add_u(expectedproduct, expectedproduct, intermediateprod);
						shr_u(intermediateovrf, num1, 512 - nrShift);
						if (prad == 1)
						{
							add_uT64(intermediateovrf, intermediateovrf, 1ull);
						};
						s16 ofad = add_u(expectedoverflow, expectedoverflow, intermediateovrf);
					};
				};


				alignas(64) u64 prod64[8];
				alignas(64)u64 ovfl64[8];
				zero_u(prod64);
				zero_u(ovfl64);
				mult_uT64(prod64, &ovfl64[7], num1, num2[7]);

				// Compare 64bit results to calculated expected results (aborts test if they don't match)
				for (int j = 0; j < 8; j++)
				{
					if (expectedproduct[j] != prod64[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedproduct[j], prod64[j],
						MSG(L"64 bit Product at " << j << " failed " << i));


					if (expectedoverflow[j] != ovfl64[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};

					Assert::AreEqual(expectedoverflow[j], ovfl64[j],
						MSG(L"64 bit Overflow at " << j << " failed " << i));
				}


				// Got a random multiplicand and multiplier. Execute function under test
				mult_u(product, overflow, num1, num2);

				// Compare results to expected (aborts test if they don't match)
				for (int j = 0; j < 8; j++)
				{
					if (expectedproduct[j] != product[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i));


					if (expectedoverflow[j] != overflow[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};

					Assert::AreEqual(expectedoverflow[j], overflow[j],
						MSG(L"Overflow at " << j << " failed " << i));
				}
			};

			string runmsg4 = "Multiply function testing. Fourth test. Multiply by sums of random powers of two, building ""expected"" 64 bit only; "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg4.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");

			// Fifth test, a multiply by sums of random powers of two into a 512 bit value. 
			// Building "expected" is a bit more complicated. 

			for (int i = 0; i < runcount; i++)
			{
				bool rbits[512];
				std::memset(rbits, 0, sizeof(rbits));

				for (int j = 0; j < 510; j++)
				{
					rbits[j] = (RandomU64(&seed) % 2) == 1;
				};

				for (int j = 7; j >= 0; j--)
				{
					num1[j] = RandomU64(&seed);
				};

				// build multiplier as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result and overflow

				u16 nrBits = (RandomU64(&seed) % 128) + 2; //  Nr Bits somewhere between 2 and 510? 
				for (int j = 0; j < nrBits; j++)
				{
					zero_u(num2);
					zero_u(expectedproduct);
					zero_u(expectedoverflow);
					zero_u(intermediateprod);
					zero_u(intermediateovrf);

					// use selected bit to build a random number (through shift/or)
					// then build expected result of multiply (through shift / add)
					bool found = false;
					u16 nrShift = 0;
					while ((nrShift == 0) && !found)
					{
						u16 rwrd = RandomU64(&seed) % 8;
						u16 rBit = RandomU64(&seed) % 64;
						for (u16 idx = rwrd * 64 + rBit; idx < 512; idx++)
						{
							u16 wt = idx / 64;
							u16 bt = idx % 64;
							u64 msk = 1ull << bt;
							if (rbits[idx] && !(num2[wt] & msk))
							{
								nrShift = idx;
								found = true;
								break;
							}
						};
						if (!found)
						{
							break;
						};
					};

					if (found)
					{
						// Multiplier:
						set_uT64(num3, 1ull);
						shl_u(num3, num3, nrShift);
						or_u(num2, num2, num3);

						// Calculate expected product and overflow:
						shl_u(intermediateprod, num1, nrShift);
						add_u(expectedproduct, expectedproduct, intermediateprod);
						shr_u(intermediateovrf, num1, 512 - nrShift);
						add_u(expectedoverflow, expectedoverflow, intermediateovrf);
					};
				};

				// Got a random multiplicand and multiplier. Execute function under test
				mult_u(product, overflow, num1, num2);

				// Compare results to expected (aborts test if they don't match)
				for (int j = 0; j < 8; j++)
				{
					if (expectedproduct[j] != product[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i));


					if (expectedoverflow[j] != overflow[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};

					Assert::AreEqual(expectedoverflow[j], overflow[j],
						MSG(L"Overflow at " << j << " failed " << i));
				}
			};

			string runmsg5 = "Multiply function testing. Fifth test. Multiply two psuedo-random 512-bit variables (use case).  "
				+ to_string(runcount) + " times.\n";;
			Logger::WriteMessage(runmsg5.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
		};

		TEST_METHOD(ui512md_01_mul_timing)
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas (64) u64 num1[8]{ 0 };
			alignas (64) u64 num2[8]{ 0 };
			alignas (64) u64 product[8]{ 0 };
			alignas (64) u64 overflow[8]{ 0 };

			for (int i = 0; i < 8; i++)
			{
				num1[i] = RandomU64(&seed);
				num2[i] = RandomU64(&seed);
			}

			for (int i = 0; i < timingcount; i++)
			{
				mult_u(product, overflow, num1, num2);
			};

			string runmsg = "Multiply function timing. Ran " +
				to_string(timingcount) + " times.\n";
			Logger::WriteMessage(runmsg.c_str());
		};

		TEST_METHOD(ui512md_02_mul64)
		{
			// mult_u tests
			// multistage testing, part for use as debugging, progressively "real" testing
			// Note: the ui512a module must pass testing before these tests, as adds are used in this test
			// Note: the ui512b module must pass testing before these tests, as 'or' and shifts are used in this test

			u64 seed = 0;
			alignas (64) u64 num1[8]{ 0 };
			alignas (64) u64 product[8]{ 0 };
			alignas (64) u64 intermediateprod[8]{ 0 };
			alignas (64) u64 expectedproduct[8]{ 0 };

			u64 num2 = 0;
			u64 num3 = 0;
			u64 overflow = 0;
			u64 intermediateovrf = 0;
			u64 expectedoverflow = 0;

			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left,
			// and expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					num1[j] = RandomU64(&seed);
				};

				num2 = 2;
				shl_u(expectedproduct, num1, u16(1));
				expectedoverflow = num1[0] >> 63;

				mult_uT64(product, &overflow, num1, num2);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i));
				};

				Assert::AreEqual(expectedoverflow, overflow, MSG(L"Overflow failed " << i));
			};

			string runmsg1 = "Multiply (u64) function testing. Simple multiply by 2 " +
				to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg1.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");

			// Second test, a simple multiply by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift left,
			// and expected overflow is a shift right
			for (u16 nrShift = 0; nrShift < 64; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < runcount / 64; i++)
				{
					for (int j = 0; j < 8; j++)
					{
						num1[j] = RandomU64(&seed);
					};

					num2 = 1ull << nrShift;
					shl_u(expectedproduct, num1, nrShift);
					expectedoverflow = (nrShift == 0) ? 0 : num1[0] >> (64 - nrShift);

					mult_uT64(product, &overflow, num1, num2);

					for (int j = 0; j < 8; j++)
					{
						Assert::AreEqual(expectedproduct[j], product[j],
							MSG(L"Product at " << j << " failed " << nrShift << i));
					}
					Assert::AreEqual(expectedoverflow, overflow, MSG(L"Overflow failed " << nrShift << " at " << i));
				};
			}

			string runmsg2 = "Multiply (u64) function testing. Multiply by sequential powers of 2 "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg2.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");

			// Third test, a multiply by random sums of powers of two. 
			// Building "expected" is a bit more complicated
			const u16 nrBits = 24;
			u16 BitsUsed[nrBits] = { 0 };
			for (int i = 0; i < runcount; i++)
			{
				u16 bitcnt = 0;
				fill_n(BitsUsed, nrBits, 0);

				for (int j = 0; j < 8; j++)
				{
					num1[j] = RandomU64(&seed);
					product[j] = 0;
					expectedproduct[j] = 0;
				};

				expectedoverflow = 0;
				overflow = 0;
				num2 = 0;
				num3 = 0;

				// build multiplier as 'N' number of random bits (avoid duplicate bits)
				// simultaneously build expected result

				u16 nrShift = 0;
				for (int j = 0; j < nrBits; j++)
				{
					bool nubit = false;
					zero_u(intermediateprod);
					intermediateovrf = 0;

					// Find a bit (0 -> 63) that hasn't already been used in this random number
					nrShift = RandomU64(&seed) % 64;
					u16 k = 0;
					while (!nubit)
					{
						u16 j2 = 0;
						for (; j2 < bitcnt; j2++)
						{
							if (nrShift == BitsUsed[j2])
							{
								break;
							}
						};
						if (nrShift == BitsUsed[j2])
						{
							nrShift = RandomU64(&seed) % 64;
						}
						else
						{
							nubit = true;
							BitsUsed[bitcnt++] = nrShift;
						};
					};

					// use selected bit to build a random number (through shift/add)
					// then build / project expected result of multiply (also through shift / add)

					// Multiplier:
					num3 = 1ull << nrShift;
					num2 += num3;

					// Expected:
					shl_u(intermediateprod, num1, nrShift);
					s32 carry = add_u(expectedproduct, expectedproduct, intermediateprod);
					intermediateovrf = (nrShift == 0) ? 0 : num1[0] >> (64 - nrShift);
					expectedoverflow += intermediateovrf;
					if (carry != 0)
					{
						expectedoverflow++;
					};
				};

				mult_uT64(product, &overflow, num1, num2);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j],
						MSG(L"Product at " << j << " failed " << i << " num2: " << num2));
				}
				Assert::AreEqual(expectedoverflow, overflow, MSG(L"Overflow failed " << i));
			};

			string runmsg3 = "Multiply (u64) function testing. Multiply by random number "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg3.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
		};

		TEST_METHOD(ui512md_02_mul64_timing)
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas (64) u64 num1[8]{ 0 };
			alignas (64) u64 product[8]{ 0 };
			u64 num2 = 0;
			u64 overflow = 0;

			for (int j = 0; j < 8; j++)
			{
				num1[j] = RandomU64(&seed);
			};

			num2 = RandomU64(&seed);

			for (int i = 0; i < timingcount; i++)
			{
				mult_uT64(product, &overflow, num1, num2);
			};

			string runmsg = "Multiply (u64) function timing. Ran " + to_string(timingcount) + " times.\n";
			Logger::WriteMessage(runmsg.c_str());
		};

		TEST_METHOD(ui512md_03_div)
		{
			u64 seed = 0;
			alignas (64) u64 num1[8]{ 0 };
			alignas (64) u64 num2[8]{ 0 };
			alignas (64) u64 dividend[8]{ 0 };
			alignas (64) u64 divisor[8]{ 0 };
			alignas (64) u64 expectedquotient[8]{ 0 };
			alignas (64) u64 expectedremainder[8]{ 0 };
			alignas (64) u64 quotient[8]{ 0 };
			alignas (64) u64 remainder[8]{ 0 };


			// First test, a simple divide by two. 
			// Easy to check as the expected answer is a shift right,
			// and expected remainder is a shift left

			for (int i = 0; i < runcount; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					dividend[j] = RandomU64(&seed);
				};

				zero_u(quotient);
				set_uT64(divisor, 2);
				shr_u(expectedquotient, dividend, u16(1));
				shl_u(expectedremainder, dividend, 511);
				shr_u(expectedremainder, expectedremainder, 511);

				div_u(quotient, remainder, dividend, divisor);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j],
						MSG(L"Quotient at " << j << " failed " << i));
					Assert::AreEqual(expectedremainder[j], remainder[j],
						MSG(L"Remainder failed " << i));
				};
			};

			string runmsg1 = "Divide function testing. Simple divide by 2 " +
				to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg1.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");


			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for (u16 nrShift = 0; nrShift < 512; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < runcount / 512; i++)
				{
					for (int j = 0; j < 8; j++)
					{
						dividend[j] = RandomU64(&seed);
					};

					set_uT64(divisor, 1);
					shl_u(divisor, divisor, nrShift);
					shr_u(expectedquotient, dividend, nrShift);
					if (nrShift == 0)
					{
						zero_u(expectedremainder);
					}
					else
					{
						u16 shft = 512 - nrShift;
						shl_u(expectedremainder, dividend, shft);
						shr_u(expectedremainder, expectedremainder, shft);
					}

					div_u(quotient, remainder, dividend, divisor);

					for (int j = 0; j < 8; j++)
					{
						Assert::AreEqual(expectedquotient[j], quotient[j],
							MSG(L"Quotient at " << j << " failed " << nrShift << i));
						Assert::AreEqual(expectedremainder[j], remainder[j],
							MSG(L"Remainder failed at " << j << " on " << nrShift << " at " << i));
					}

				};
			}

			string runmsg2 = "Divide function testing. Divide by sequential powers of 2 "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg2.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");

			//	Use case testing
			//		Divide number by common use case examples

			int adjruncount = runcount / 64;
			for (int i = 0; i < adjruncount; i++)
			{
				for (int m = 7; m >= 0; m--)
				{
					for (int j = 7; j >= 0; j--)
					{
						for (int l = 0; l < 8; l++)
						{
							num1[l] = RandomU64(&seed);
							num2[l] = 0;
							quotient[l] = 0;
							remainder[l] = 0;
						};
						num2[m] = 1;
						;
						div_u(quotient, remainder, num1, num2);


						for (int v = 7; v >= 0; v--)
						{
							int qidx, ridx = 0;
							u64 qresult, rresult = 0;

							qidx = v - (7 - m);
							qresult = (qidx >= 0) ? qresult = (v >= (7 - m)) ? num1[qidx] : 0ull : qresult = 0;
							rresult = (v > m) ? num1[v] : 0ull;

							Assert::AreEqual(quotient[v], qresult, L"Quotient incorrect");
							Assert::AreEqual(remainder[v], rresult, L" Remainder incorrect");
						};

						num2[m] = 0;
					};
				};
			};
			string runmsg = "Divide function testing. Ran tests "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
		};

		TEST_METHOD(ui512md_03_div_timing)
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas (64) u64 dividend[8]{ 0 };
			alignas (64) u64 quotient[8]{ 0 };
			alignas (64) u64 divisor[8]{ 0 };
			alignas (64) u64 remainder[8]{ 0 };

			for (int i = 0; i < 8; i++)
			{
				dividend[i] = RandomU64(&seed);
				divisor[i] = RandomU64(&seed);
			}
			zero_u(quotient);
			zero_u(remainder);

			for (int i = 0; i < timingcount; i++)
			{
				div_u(quotient, remainder, dividend, divisor);
			};

			string runmsg = "Divide function timing. Ran "
				+ to_string(timingcount) + " times.\n";
			Logger::WriteMessage(runmsg.c_str());
		};
		TEST_METHOD(ui512md_04_div64)
		{
			u64 seed = 0;
			alignas (64) u64 dividend[8]{ 0 };
			alignas (64) u64 quotient[8]{ 0 };
			alignas (64) u64 expectedquotient[8]{ 0 };
			u64 divisor = 0;
			u64 remainder = 0;
			u64 expectedremainder = 0;


			// First test, a simple divide by two. 
			// Easy to check as the expected answer is a shift right,
			// and expected remainder is a shift left

			for (int i = 0; i < runcount; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					dividend[j] = RandomU64(&seed);
				};
				zero_u(quotient);

				divisor = 2;
				shr_u(expectedquotient, dividend, u16(1));
				expectedremainder = (dividend[7] << 63) >> 63;

				div_uT64(quotient, &remainder, dividend, divisor);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j],
						MSG(L"Quotient at " << j << " failed " << i));
				};

				Assert::AreEqual(expectedremainder, remainder, MSG(L"Remainder failed " << i));
			};

			string runmsg1 = "Divide (u64) function testing. Simple divide by 2 " +
				to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg1.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");


			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for (u16 nrShift = 0; nrShift < 64; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < runcount / 64; i++)
				{
					for (int j = 0; j < 8; j++)
					{
						dividend[j] = RandomU64(&seed);
					};

					divisor = 1ull << nrShift;
					shr_u(expectedquotient, dividend, nrShift);
					expectedremainder = (nrShift == 0) ? 0 : (dividend[7] << (64 - nrShift)) >> (64 - nrShift);

					div_uT64(quotient, &remainder, dividend, divisor);

					for (int j = 0; j < 8; j++)
					{
						Assert::AreEqual(expectedquotient[j], quotient[j],
							MSG(L"Quotient at " << j << " failed " << nrShift << i));
					}
					Assert::AreEqual(expectedremainder, remainder, MSG(L"Remainder failed " << nrShift << " at " << i));
				};
			}

			string runmsg2 = "Divide function testing. Divide by sequential powers of 2 "
				+ to_string(runcount) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg2.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");


			// Third test, Use case tests, divide out to get decimal digits. Do whole with random,
			// and a knowable sample 
			{
				string digits = "";

				for (int j = 0; j < 8; j++)
				{
					dividend[j] = RandomU64(&seed);
				};

				int comp = compare_uT64(dividend, 0ull);
				int cnt = 0;
				while (comp != 0)
				{
					div_uT64(dividend, &remainder, dividend, 10ull);
					char digit = 0x30 + char(remainder);
					digits.insert(digits.begin(), digit);
					comp = compare_uT64(dividend, 0ull);
					if (comp != 0)
					{
						cnt++;
						if (cnt % 30 == 0)
						{
							digits.insert(digits.begin(), '\n');
						}
						else
						{
							if (cnt % 3 == 0)
							{
								digits.insert(digits.begin(), ',');
							};
						};
					}

				}
				Logger::WriteMessage(L"Use case: Divide to extract decimal digits:\n");
				Logger::WriteMessage(digits.c_str());
			}

			{
				string digits = "";
				u64 num = 12345678910111213ull;
				set_uT64(dividend, num);

				int comp = compare_uT64(dividend, 0ull);
				int cnt = 0;
				while (comp != 0)
				{
					div_uT64(dividend, &remainder, dividend, 10ull);
					char digit = 0x30 + char(remainder);
					digits.insert(digits.begin(), digit);
					comp = compare_uT64(dividend, 0ull);
					if (comp != 0)
					{
						cnt++;
						if (cnt % 3 == 0)
						{
							digits.insert(digits.begin(), ',');
						};
					}

				}
				string expected = "12,345,678,910,111,213";
				Assert::AreEqual(expected, digits);
				Logger::WriteMessage(L"\n\nUse case: Divide to extract known decimal digits:\n(Validated via assert)\n");
				Logger::WriteMessage(digits.c_str());
			}
		}

		TEST_METHOD(ui512md_04_div64_timing)
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			alignas (64) u64 dividend[8]{ 0 };
			alignas (64) u64 quotient[8]{ 0 };
			u64 divisor = 0;
			u64 remainder = 0;

			for (int i = 0; i < 8; i++)
			{
				dividend[i] = RandomU64(&seed);
			}
			zero_u(quotient);
			divisor = RandomU64(&seed);

			for (int i = 0; i < timingcount; i++)
			{
				div_uT64(quotient, &remainder, dividend, divisor);
			};

			string runmsg = "Divide by u64  function timing. Ran "
				+ to_string(timingcount) + " times.\n";
			Logger::WriteMessage(runmsg.c_str());
		};
	};
};
