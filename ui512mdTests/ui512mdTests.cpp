//		ui51mdTests
// 
//		File:			ui512mdTests.cpp
//		Author:			John G.Lynch
//		Legal:			Copyright @2024, per MIT License below
//		Date:			June 19, 2024
//
//		This sub - project: ui512mdTests, is a unit test project that invokes each of the routines in the ui512md assembly.
//		It runs each assembler proc with pseudo - random values.
//		It validates ( asserts ) expected and returned results.
//		It also runs each repeatedly for comparative timings.
//		It provides a means to invoke and debug.
//		It illustrates calling the routines from C++.

#include "pch.h"
#include "CppUnitTest.h"
#include "ui512a.h"
#include "ui512b.h"
#include "ui512md.h"
#include "CommonTypeDefs.h"

#include <cstring>
#include <sstream>
#include <format>
#include <chrono>

using namespace std;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ui512mdTests
{
	TEST_CLASS(ui512mdTests)
	{
	public:

		const s32 runcount = 1000;
		const s32 regvercount = 5000;
		const s32 timingcount = 1000000;
		const s32 timingcount_short = 50000;
		const s32 timingcount_medium = 150000;
		const s32 timingcount_long = 300000;

		/// <summary>
		/// Random number generator
		/// uses linear congruential method 
		/// ref: Knuth, Art Of Computer Programming, Vol. 2, Seminumerical Algorithms, 3rd Ed. Sec 3.2.1
		/// </summary>
		/// <param name="seed">if zero, will supply with: 4294967291</param>
		/// <returns>Pseudo-random number from zero to ~2^63 (9223372036854775807)</returns>
		u64 RandomU64(u64* seed)
		{
			const u64 m = 18446744073709551557ull;			// greatest prime below 2^64
			const u64 a = 68719476721ull;					// closest prime below 2^36
			const u64 c = 268435399ull;						// closest prime below 2^28
			// suggested seed: around 2^32, 4294967291
			*seed = (*seed == 0ull) ? (a * 4294967291ull + c) % m : (a * *seed + c) % m;
			return *seed;
		};

		/// <summary>
		/// Random fill of ui512 variable
		/// </summary>
		void RandomFill(u64* var, u64* seed)
		{
			for (int i = 0; i < 8; i++)
			{
				var[i] = RandomU64(seed);
			};
		};
		TEST_METHOD(random_number_generator)
		{
			//	Check distribution of "random" numbers
			u64 seed = 0;
			const u32 dec = 10;
			u32 dist[dec]{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			const u64 split = 18446744073709551557ull / dec;
			u32 distc = 0;
			float varsum = 0.0;
			float deviation = 0.0;
			float D = 0.0;
			float sumD = 0.0;
			float variance = 0.0;
			const u32 randomcount = 1000000;
			const s32 norm = randomcount / dec;
			// generate random numbers, count distribution
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

			// evaluate distribution
			for (int i = 0; i < 10; i++)
			{
				deviation = float(abs(long(norm) - long(dist[i])));
				D = (deviation * deviation) / float(long(norm));
				sumD += D;
				variance = float(deviation) / float(norm) * 100.0f;
				varsum += variance;
				msgd += format("\t{:6d}", dist[i]);
				msgv += format("\t{:5.3f}% ", variance);
				msgchi += format("\t{:5.3f}% ", D);
				distc += dist[i];
			};

			msgd += "\t\tDecile counts sum to: " + to_string(distc) + "\n";
			Logger::WriteMessage(msgd.c_str());
			msgv += "\t\tVariance sums to: ";
			msgv += format("\t{:6.3f}% ", varsum);
			msgv += '\n';
			Logger::WriteMessage(msgv.c_str());
			msgchi += "\t\tChi-squared distribution: ";
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

			u64 seed = 0;			// seed of zero will initialize to a known value
			regs r_before{};
			regs r_after{};
			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(num3) { 0 };
			_UI512(product) { 0 };
			_UI512(overflow) { 0 };
			_UI512(intermediateprod) { 0 };
			_UI512(intermediateovrf) { 0 };
			_UI512(expectedproduct) { 0 };
			_UI512(expectedoverflow) { 0 };
			_UI512(work) { 0 };

			// Edge case tests

			// 1. zero times zero (don't loop and repeat, just once)
			zero_u(num1);
			zero_u(num2);
			zero_u(expectedproduct);
			zero_u(expectedoverflow);
			reg_verify((u64*)&r_before);
			s16 ret = mult_u(product, overflow, num1, num2);
			reg_verify((u64*)&r_after);
			Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			Assert::AreEqual(s16(0), ret, L"Return code failed zero times zero test."); // Only exception possible is parameter alignment
			for (int j = 0; j < 8; j++)
			{
				Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed zero times zero test."));
				Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed zero times zero test."));
			};

			// 2. zero times random
			for (int i = 0; i < runcount; i++)
			{
				zero_u(num1);
				RandomFill(num2, &seed);
				zero_u(expectedproduct);
				zero_u(expectedoverflow);
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed zero times random test."); // Only exception possible is parameter alignment
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed zero times random test on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed zero times random test on run #" << i));
				};
			};

			// 3. random times zero
			for (int i = 0; i < runcount; i++)
			{
				zero_u(num2);
				RandomFill(num1, &seed);
				zero_u(expectedproduct);
				zero_u(expectedoverflow);
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random times zero test."); // Only exception possible is parameter alignment
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed random times zero test on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed random times zero test on run #" << i));
				};
			};

			// 4. one times random
			for (int i = 0; i < runcount; i++)
			{
				set_uT64(num1, 1ull);
				RandomFill(num2, &seed);
				copy_u(expectedproduct, num2);
				zero_u(expectedoverflow);
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed one times random test."); // Only exception possible is parameter alignment
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed one times random test on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed one times random test on run #" << i));
				};
			};

			// 5. random times one
			for (int i = 0; i < runcount; i++)
			{
				set_uT64(num2, 1ull);
				RandomFill(num1, &seed);
				copy_u(expectedproduct, num1);
				zero_u(expectedoverflow);
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random times one test."); // Only exception possible is parameter alignment
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed random times one test on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed random times one test on run #" << i));
				};
			};

			{
				string test_message = _MSGA("Multiply function testing.\nEdge cases: zero times zero, zero times random, random times zero, one times random, random times one. "
					<< runcount << " times each, with pseudo random values.\n";);
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n");
			};


			// Now the real tests, progressively more complex

			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left, expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(num1, &seed);					//	random initialize multiplicand				
				set_uT64(num2, 2ull);						//	initialize multiplier				
				shl_u(expectedproduct, num1, u16(1));		// calculate expected product				
				shr_u(expectedoverflow, num1, u16(511));	// calculate expected overflow				
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed simple times two test."); // Only exception possible is parameter alignment

				//	check actual vs. expected
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed on run #" << i));
				}
			};
			{
				string test_message = _MSGA("First test, a simple multiply by two.  " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n");
			};

			// Second test, a simple multiply by a random power of two. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(num1, &seed);
				set_uT64(num2, 1ull);
				u16 nrShift = RandomU64(&seed) % 512;
				shl_u(num2, num2, nrShift);
				shl_u(expectedproduct, num1, nrShift);
				shr_u(expectedoverflow, num1, (512 - nrShift));
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random power of 2 test."); // Only exception possible is parameter alignment
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed on run #" << i));
				}
			};

			{
				string test_message = _MSGA("Second test, a simple multiply by a random power of two. " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n");
			};

			// Third test, a simple multiply by a random 64 bit integer. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(num1, &seed);
				u64 num2_64 = RandomU64(&seed);
				u64 expectedovfl_64 = 0;
				set_uT64(num2, num2_64);
				mult_uT64(expectedproduct, &expectedovfl_64, num1, num2_64);
				set_uT64(expectedoverflow, expectedovfl_64);
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random 64 test."); // Only exception possible is parameter alignment

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i));
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed on run #" << i));
				}
			};

			{
				string test_message = _MSGA("Third test, a multiply by a random 64 bit value. " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n");
			};

			// Fourth test, a multiply by sums of random powers of two into a 64 bit value. 
			// Building "expected" is a bit more complicated. This test is more about correctly building "expected"
			for (int i = 0; i < runcount; i++)
			{
				bool rbits[64];
				std::memset(rbits, 0, sizeof(rbits));

				for (int j = 0; j < 63; j++)
				{
					rbits[j] = (RandomU64(&seed) % 2) == 1;
				};

				// random initialize multiplicand
				RandomFill(num1, &seed);

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

				_UI512(prod64);
				_UI512(ovfl64);
				zero_u(prod64);
				zero_u(ovfl64);
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(prod64, ovfl64, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random 64 test."); // Only exception possible is parameter alignment

				// Compare 64bit results to calculated expected results (aborts test if they don't match)
				for (int j = 0; j < 8; j++)
				{
					if (expectedproduct[j] != prod64[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedproduct[j], prod64[j], _MSGW(L"64 bit Product at word #" << j << " failed on run #" << i));
					if (expectedoverflow[j] != ovfl64[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedoverflow[j], ovfl64[j], _MSGW(L"64 bit Overflow at word #" << j << " failed on run #" << i));
				}

				// Got a random multiplicand and multiplier. Execute function under test
				reg_verify((u64*)&r_before);
				s16 ret2 = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret2, L"Return code failed random 64 test."); // Only exception possible is parameter alignment

				// Compare results to expected (aborts test if they don't match)
				for (int j = 0; j < 8; j++)
				{
					if (expectedproduct[j] != product[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i));
					if (expectedoverflow[j] != overflow[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed on run #" << i));
				}
			};

			{
				string test_message = _MSGA("Fourth test. Multiply by sums of random powers of two, building ""expected"" 64 bit only; " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n");
			}

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

				RandomFill(num1, &seed);

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
				reg_verify((u64*)&r_before);
				s16 ret = mult_u(product, overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed zero times zero test."); // Only exception possible is parameter alignment

				// Compare results to expected (aborts test if they don't match)
				for (int j = 0; j < 8; j++)
				{
					if (expectedproduct[j] != product[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i));
					if (expectedoverflow[j] != overflow[j]) {
						Logger::WriteMessage(ToString(j).c_str());
					};
					Assert::AreEqual(expectedoverflow[j], overflow[j], _MSGW(L"Overflow at word #" << j << " failed on run #" << i));
				}
			};
			{
				string test_message = _MSGA("Fifth test. Multiply by sums of random powers of two, building ""expected"" full 512 bit; " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n");
			};
		};

		TEST_METHOD(ui512md_01_mul_timing)
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(product) { 0 };
			_UI512(overflow) { 0 };

			for (int i = 0; i < 8; i++)
			{
				num1[i] = RandomU64(&seed);
				num2[i] = RandomU64(&seed);
			}

			for (int i = 0; i < timingcount; i++)
			{
				mult_u(product, overflow, num1, num2);
			};
			{
				string test_message = _MSGA("Multiply function timing test. Ran " << timingcount << " times.\n");
				Logger::WriteMessage(test_message.c_str());
			};
		};

		TEST_METHOD(ui512md_01_mul_performance_timing)
		{
			// Performance timing tests.
			// Run in three batches, short, medium, long
			// Short run, 100,000 times. Use first batch to determine mean / min / max
			// Medium run, 250,000 times. Using mean / min / max: determine variance from mean, standard of deviation
			// Long run, 500,000 times. Count occurrences by variance bands from mean, using standard of deviation, saving outliers for further examination
			// Note: the timingcount_short, timingcount_medium, and timingcount_long constants are defined at the head of this class

			double total_short = 0.0;
			double min_short = 1000000.0;
			double max_short = 0.0;
			double mean_short = 0.0;

			double total_medium = 0.0;
			double min_medium = 1000000.0;
			double max_medium = 0.0;
			double mean_medium = 0.0;
			double variance_medium = 0.0;
			double stddev_medium = 0.0;

			double total_long = 0.0;
			double min_long = 1000000.0;
			double max_long = 0.0;
			double mean_long = 0.0;
			double variance_long = 0.0;
			double stddev_long = 0.0;
			int varbands_long[9]{ 0,0,0,0,0,0,0,0,0 };
			// bands: < -3 stddev, -3 to -2 stddev, -2 to -1 stddev, -1 to mean, mean to +1 stddev, +1 to +2 stddev, +2 to +3 stddev, > +3 stddev
			const int outlier_threshold = 5; // microseconds

			struct outlier
			{
				_UI512(num1);
				_UI512(num2);
				_UI512(product);
				_UI512(overflow);
				s16 returncode;
				int iteration;
				double duration;
				double variance;
			};
			vector<outlier> outliers;

			struct instance
			{
				double deviation;
			};

			// First batch, short run
			// determine mean / min / max
			{
				u64 seed = 0;
				_UI512(num1) { 0 };
				_UI512(num2) { 0 };
				_UI512(product) { 0 };
				_UI512(overflow) { 0 };

				for (int i = 0; i < timingcount_short; i++)
				{
					RandomFill(num1, &seed);
					RandomFill(num2, &seed);
					auto countStart = std::chrono::high_resolution_clock::now();
					mult_u(product, overflow, num1, num2);
					auto countEnd = std::chrono::high_resolution_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double countDurus = countDur.count();
					min_short = (countDurus < min_short) ? countDurus : min_short;
					max_short = (countDurus > max_short) ? countDurus : max_short;
					total_short += countDurus;
				};
				{
					double mean_short = total_short / double(timingcount_short);
					string test_message = _MSGA("Multiply function performance timing test.\nFirst batch. \nRan "
						<< timingcount_short << " times.\nTotal execution time: "
						<< total_short / 1000 << " milliseconds. \nAverage time per call : "
						<< mean_short << " microseconds.\nMinimum in "
						<< min_short << " microseconds.\nMaximum in "
						<< max_short << " microseconds.\n");
					Logger::WriteMessage(test_message.c_str());
				};
			}

			// Second batch, medium run
			// Using mean / min / max: determine variance from mean, standard of deviation
			{
				for (int i = 0; i < timingcount_medium; i++)
				{
					u64 seed = 0;
					_UI512(num1) { 0 };
					_UI512(num2) { 0 };
					_UI512(product) { 0 };
					_UI512(overflow) { 0 };

					RandomFill(num1, &seed);
					RandomFill(num2, &seed);
					auto countStart = std::chrono::high_resolution_clock::now();
					mult_u(product, overflow, num1, num2);
					auto countEnd = std::chrono::high_resolution_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double countDurus = countDur.count();
					min_medium = (countDurus < min_medium) ? countDurus : min_medium;
					max_medium = (countDurus > max_medium) ? countDurus : max_medium;
					total_medium += countDurus;
				};
				{
					u64 seed = 0;
					_UI512(num1) { 0 };
					_UI512(num2) { 0 };
					_UI512(product) { 0 };
					_UI512(overflow) { 0 };
					mean_medium = total_medium / double(timingcount_medium);
					double varsum = 0.0;
					variance_medium = 0.0;
					stddev_medium = 0.0;
					for (int i = 0; i < timingcount_medium; i++)
					{
						RandomFill(num1, &seed);
						RandomFill(num2, &seed);
						auto countStart = std::chrono::high_resolution_clock::now();
						mult_u(product, overflow, num1, num2);
						auto countEnd = std::chrono::high_resolution_clock::now();
						std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
						double countDurus = countDur.count();
						double var = mean_medium - countDurus;
						varsum += var * var;
					};
					variance_medium = varsum / double(timingcount_medium);
					stddev_medium = sqrt(variance_medium);
					string test_message = _MSGA("\nSecond batch. \nRan "
						<< timingcount_medium << " times.\nTotal execution time: "
						<< total_medium / 1000 << " milliseconds. \nAverage time per call : "
						<< mean_medium << " microseconds.\nMinimum in "
						<< min_medium << " microseconds.\nMaximum in "
						<< max_medium << " microseconds.\nVariance: "
						<< variance_medium << " microseconds.\nStandard Deviation: "
						<< stddev_medium << " microseconds.\n");
					Logger::WriteMessage(test_message.c_str());
				};

				// Third batch, long run
				// Count occurrences by variance bands from mean, using standard of deviation
				{
					double sdev_5 = 0.5 * stddev_medium;
					double sdev1_5 = 1.5 * stddev_medium;
					double sdev2_5 = 2.5 * stddev_medium;
					double sdev3_5 = 3.5 * stddev_medium;
					for (int i = 0; i < timingcount_long; i++)
					{
						u64 seed = 0;
						_UI512(num1) { 0 };
						_UI512(num2) { 0 };
						_UI512(product) { 0 };
						_UI512(overflow) { 0 };
						RandomFill(num1, &seed);
						RandomFill(num2, &seed);
						auto countStart = std::chrono::high_resolution_clock::now();
						s16 ret = mult_u(product, overflow, num1, num2);
						auto countEnd = std::chrono::high_resolution_clock::now();
						std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
						double countDurus = countDur.count();
						min_long = (countDurus < min_long) ? countDurus : min_long;
						max_long = (countDurus > max_long) ? countDurus : max_long;
						total_long += countDurus;
						double var = countDurus - mean_medium;
						double absvar = (var < 0.0) ? -var : var;

						if (absvar <= sdev_5)
						{
							varbands_long[4]++;
						}
						else if (var <= -sdev_5 && var > -sdev1_5)
						{
							varbands_long[3]++;
						}
						else if (var <= -sdev1_5 && var > -sdev2_5)
						{
							varbands_long[2]++;
						}
						else if (var = -sdev2_5 && var > -sdev3_5)
						{
							varbands_long[1]++;
						}
						else if (var <= -sdev3_5)
						{
							varbands_long[0]++;
						}
						else if (var >= sdev_5 && var < sdev1_5)
						{
							varbands_long[5]++;
						}
						else if (var >= sdev1_5 && var <= sdev2_5)
						{
							varbands_long[6]++;
						}
						else if (var >= sdev2_5 && var <= sdev3_5)
						{
							varbands_long[7]++;
						}
						else if (var >= sdev3_5)
						{
							varbands_long[7]++;
						};

						if (absvar > sdev3_5)
						{
							outlier ol;
							copy_u(ol.num1, num1);
							copy_u(ol.num2, num2);
							copy_u(ol.product, product);
							copy_u(ol.overflow, overflow);
							ol.returncode = ret;
							ol.iteration = i;
							ol.duration = countDurus;
							ol.variance = var;
							outliers.push_back(ol);
						}
					}

					string test_message = _MSGA("\n\nThird batch. \nRan "
						<< timingcount_long << " times.\nTotal execution time: "
						<< total_long / 1000 << " milliseconds. \nAverage time per call : "
						<< mean_medium << " microseconds.\nMinimum in "
						<< min_long << " microseconds.\nMaximum in "
						<< max_long << " microseconds.\nVariance: "
						<< variance_medium << " microseconds.\nStandard Deviation: "
						<< stddev_medium << " microseconds.\n"
						<< "Variance bands:\n"
						<< "< -3.5 stddev: " << varbands_long[0] << "\n"
						<< "-3.5 to -2.5 stddev: " << varbands_long[1] << "\n"
						<< "-2.5 to -1.5 stddev: " << varbands_long[2] << "\n"
						<< "-1.5 to -0.5 stddev: " << varbands_long[3] << "\n"
						<< "-0.5 to +0.5 stddev: " << varbands_long[4] << "\n"
						<< "+0.5 to +1.5 stddev: " << varbands_long[5] << "\n"
						<< "+1.5 to +2.5 stddev: " << varbands_long[6] << "\n"
						<< "+2.5 to +3.5 stddev: " << varbands_long[7] << "\n"
						<< "> +3.5 stddev: " << varbands_long[8] << "\n\n"
						<< "Outliers (deviation more than " << sdev3_5 << " microseconds from mean): " << outliers.size() << "\n\n");
					Logger::WriteMessage(test_message.c_str());

					if (outliers.size() > 0)
					{
						int olcnt = 0;
						for (auto ol : outliers)
						{
							if (olcnt < outlier_threshold)
							{
								string test_message = _MSGA("Outlier #" << olcnt << " iteration " << ol.iteration << " duration "
									<< ol.duration << " microseconds, variance " << ol.variance << " return code " << ol.returncode << "\n"
									<< "num1: " << _MtoHexString(ol.num1) << "\n"
									<< "num2: " << _MtoHexString(ol.num2) << "\n"
									<< "product: " << _MtoHexString(ol.product) << "\n"
									<< "overflow: " << _MtoHexString(ol.overflow) << "\n\n"
								);
								Logger::WriteMessage(test_message.c_str());
							};
							olcnt++;
						};
						if (outliers.size() > outlier_threshold)
						{
							string test_message = _MSGA("... plus " << (outliers.size() - outlier_threshold) << " more outliers.\n");
							Logger::WriteMessage(test_message.c_str());
						};
					}
					else
					{
						string test_message = _MSGA("No outliers.\n");
						Logger::WriteMessage(test_message.c_str());
					};
				}
			}
		}


		TEST_METHOD(ui512md_01_mul_pnv)
		{
			// Path and non-volatile reg tests

			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(product) { 0 };
			_UI512(overflow) { 0 };
			regs r_before{};
			regs r_after{};

			for (int i = 0; i < 8; i++)
			{
				num1[i] = RandomU64(&seed);
				num2[i] = RandomU64(&seed);
			}

			for (int i = 0; i < runcount; i++)
			{
				r_before.Clear();
				reg_verify((u64*)&r_before);
				mult_u(product, overflow, num1, num2);
				r_after.Clear();
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			};

			{
				string test_message = _MSGA("Multiply function: path and non-volatile reg tests. Ran " << runcount << " times.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
		};
		TEST_METHOD(ui512md_02_mul64)
		{
			// mult_uT64 tests
			// multistage testing, part for use as debugging, progressively "real" testing
			// Note: the ui512a module must pass testing before these tests, as adds are used in this test
			// Note: the ui512b module must pass testing before these tests, as 'or' and shifts are used in this test

			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(product) { 0 };
			_UI512(intermediateprod) { 0 };
			_UI512(expectedproduct) { 0 };

			u64 num2 = 0;
			u64 num3 = 0;
			u64 overflow = 0;
			u64 intermediateovrf = 0;
			u64 expectedoverflow = 0;


			// Edge case tests
			// 
			// 1. zero times zero
			zero_u(num1);
			num2 = 0;
			zero_u(expectedproduct);
			expectedoverflow = 0;
			mult_uT64(product, &overflow, num1, num2);
			Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed zero times zero"));
			for (int j = 0; j < 8; j++)
			{
				Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed zero times zero"));
			};

			// 2. zero times random
			for (int i = 0; i < runcount; i++)
			{
				zero_u(num1);
				num2 = RandomU64(&seed);
				zero_u(expectedproduct);
				expectedoverflow = 0;
				mult_uT64(product, &overflow, num1, num2);
				Assert::AreEqual(expectedoverflow, overflow,
					_MSGW(L"Overflow  failed zero times random " << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed zero times random on run #" << i));
				};
			};

			// 3. random times zero
			for (int i = 0; i < runcount; i++)
			{
				num2 = 0;
				RandomFill(num1, &seed);
				zero_u(expectedproduct);
				expectedoverflow = 0;
				mult_uT64(product, &overflow, num1, num2);
				Assert::AreEqual(expectedoverflow, overflow,
					_MSGW(L"Overflow failed random times zero " << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed random times zero on run #" << i));
				};
			};

			// 4. one times random
			for (int i = 0; i < runcount; i++)
			{
				set_uT64(num1, 1ull);
				num2 = RandomU64(&seed);
				set_uT64(expectedproduct, num2);
				expectedoverflow = 0;
				mult_uT64(product, &overflow, num1, num2);
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed one times random on run #" << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed one times random on run #" << i));
				};
			};

			// 5. random times one
			for (int i = 0; i < runcount; i++)
			{
				num2 = 1ull;
				RandomFill(num1, &seed);
				copy_u(expectedproduct, num1);
				expectedoverflow = 0;
				mult_uT64(product, &overflow, num1, num2);
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed random times one on run #" << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed random times one on run #" << i));
				};
			};

			{
				string test_message = _MSGA("Multiply function testing. Edge cases: zero times zero, zero times random, random times zero, one times random, random times one. \n "
					<< runcount << " times each, with pseudo random values.\n";);
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n");
			};

			// Now the real tests, progressively more complex
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
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on ru #" << i));
				};
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed " << i));
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
					RandomFill(num1, &seed);
					num2 = 1ull << nrShift;
					shl_u(expectedproduct, num1, nrShift);
					expectedoverflow = (nrShift == 0) ? 0 : num1[0] >> (64 - nrShift);

					mult_uT64(product, &overflow, num1, num2);

					for (int j = 0; j < 8; j++)
					{
						Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed shift: " << nrShift << " on run #" << i));
					}
					Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed shift: " << nrShift << " on run #" << i));
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
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i << " num2: " << num2));
				}
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed " << i));
			};
			{
				string test_message = _MSGA("Multiply function testing. Third test. Multiply by sums of random powers of two, building ""expected""; "
					<< runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n");
			}
		};

		TEST_METHOD(ui512md_02_mul64_timing)
		{
			// Timing test. Eliminate everything but execution of the subject function
			// other than set-up, and messaging complete.

			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(product) { 0 };
			u64 num2 = 0;
			u64 overflow = 0;
			RandomFill(num1, &seed);
			num2 = RandomU64(&seed);

			for (int i = 0; i < timingcount; i++)
			{
				mult_uT64(product, &overflow, num1, num2);
			};
			{
				string test_message = _MSGA("Multiply (u64) function timing. Ran "
					<< timingcount << " times with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
			};
		};

		TEST_METHOD(ui512md_02_mul64_pnv)
		{
			// Path and non-volatile reg tests

			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(product) { 0 };
			u64 num2{ 0 };
			u64 overflow{ 0 };
			regs r_before{};
			regs r_after{};
			RandomFill(num1, &seed);
			num2 = RandomU64(&seed);
			for (int i = 0; i < runcount; i++)
			{
				reg_verify((u64*)&r_before);
				mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			};
			{
				string test_message = _MSGA("Multiply (u64) function:  path and non-volatile reg tests. "
					<< runcount << " times.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
		};


		TEST_METHOD(ui512md_03_div_pt1)
		{
			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(dividend) { 0 };
			_UI512(divisor) { 0 };
			_UI512(expectedquotient) { 0 };
			_UI512(expectedremainder) { 0 };
			_UI512(quotient) { 0 };
			_UI512(remainder) { 0 };
			regs r_before{};
			regs r_after{};

			// Edge case tests

			// 1. zero divided by random
			zero_u(dividend);
			for (int i = 0; i < runcount; i++)
			{
				zero_u(dividend);
				RandomFill(divisor, &seed);
				zero_u(expectedquotient);
				zero_u(expectedremainder);
				reg_verify((u64*)&r_before);
				s16 retcode = div_u(quotient, remainder, dividend, divisor);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at word #" << j << " failed zero divided by random on run #" << i));
					Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder at word #" << j << " failed zero divided by random on run #" << i));
				};
				Assert::AreEqual(s16(0), retcode, L"Return code failed zero divided by random");
			};

			// 2.random divided by zero
			zero_u(dividend);
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(dividend, &seed);
				zero_u(divisor);
				zero_u(expectedquotient);
				zero_u(expectedremainder);
				reg_verify((u64*)&r_before);
				s16 retcode = div_u(quotient, remainder, dividend, divisor);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at word #" << j << " failed random divided by zero on run #" << i));
					Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder at word #" << j << " failed random divided by zero on run #" << i));
				};
				Assert::AreEqual(s16(-1), retcode, L"Return code failed random divided by zero");
			};

			// 3. random divided by one
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(dividend, &seed);
				set_uT64(divisor, 1);
				copy_u(expectedquotient, dividend);
				zero_u(expectedremainder);
				reg_verify((u64*)&r_before);
				s16 retcode = div_u(quotient, remainder, dividend, divisor);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at word #" << j << " failed random divided by one on run #" << i));
					Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder at word #" << j << " failed random divided by one on run #" << i));
				};
				Assert::AreEqual(s16(0), retcode, L"Return code failed random divided by one");
			};

			// 4. one divided by random
			for (int i = 0; i < runcount; i++)
			{
				zero_u(dividend);
				set_uT64(dividend, 1);
				RandomFill(divisor, &seed);
				zero_u(expectedquotient);
				copy_u(expectedremainder, divisor);
				reg_verify((u64*)&r_before);
				s16 retcode = div_u(quotient, remainder, dividend, divisor);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), retcode, L"Return code failed one divided by random");
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at word #" << j << " failed one divided by random on run #" << i));
					//	Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder at word #" << j << " failed one divided by random on run #" << i));
				};
			};

			// 5. random divided by single word divisor, random bit 0->63
			// expected quotient is a shift right, expected remainder is a shift left
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(dividend, &seed);
				u16 bitno = RandomU64(&seed) % 63; // bit 0 to 63
				u64 divby = 1ull << bitno;
				set_uT64(divisor, divby);
				shr_u(expectedquotient, dividend, bitno);
				shl_u(expectedremainder, dividend, 512 - bitno);
				shr_u(expectedremainder, expectedremainder, 512 - bitno);
				reg_verify((u64*)&r_before);
				s16 retcode = div_u(quotient, remainder, dividend, divisor);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), retcode, L"Return code failed one divided by random");
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at word #" << j << " failed random divided by one word of random bit during run #" << i));
					Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder at word #" << j << " failed random divided by one word of random bit during run #" << i));
				};
			};

			{
				string test_message = _MSGA("Divide function testing.\n Edge cases: zero divided by random, random divided by zero, random divided by one"
					"one divided by random, random divided by one word of random bit.\n"
					<< runcount << " times each, with pseudo random values. Non-volatile registers verified.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			};

		};

		TEST_METHOD(ui512md_03_div_pt2)
		{
			u64 seed = 0;
			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(dividend) { 0 };
			_UI512(divisor) { 0 };
			_UI512(expectedquotient) { 0 };
			_UI512(expectedremainder) { 0 };
			_UI512(quotient) { 0 };
			_UI512(remainder) { 0 };
			regs r_before{};
			regs r_after{};

			//	Pre-test: various sizes of dividend / divisor
			//	Just to exercise various paths through the code

			s16 retval = 0;
			//	Pre-testing, various sizes of dividend / divisor
			for (int i = 7; i >= 0; i--)
			{
				for (int j = 7; j >= 0; j--)
				{
					zero_u(dividend);
					zero_u(divisor);
					dividend[i] = RandomU64(&seed);
					divisor[j] = RandomU64(&seed);
					if ((i == 5 && j == 6) || (i == 6 && j == 7)) {
						break;
					}
					reg_verify((u64*)&r_before);
					retval = div_u(quotient, remainder, dividend, divisor);
					reg_verify((u64*)&r_after);
					Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				};
			};

			// First test, a simple divide by two. 
			// Easy to check as the expected answer is a shift right,
			// and expected remainder is a shift left

			for (int i = 0; i < runcount; i++)
			{
				RandomFill(dividend, &seed);
				zero_u(quotient);
				set_uT64(divisor, 2);
				shr_u(expectedquotient, dividend, u16(1));
				shl_u(expectedremainder, dividend, 511);
				shr_u(expectedremainder, expectedremainder, 511);

				div_u(quotient, remainder, dividend, divisor);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at " << j << " failed " << i));
					Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder failed " << i));
				};
			};

			{
				string test_message = _MSGA("Divide function testing. Simple divide by 2 " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for (u16 nrShift = 0; nrShift < 512; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < runcount / 512; i++)
				{
					RandomFill(dividend, &seed);
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
						Assert::AreEqual(expectedquotient[j], quotient[j], _MSGW(L"Quotient at " << j << " failed " << nrShift << " at " << i));
						Assert::AreEqual(expectedremainder[j], remainder[j], _MSGW(L"Remainder failed at " << j << " on " << nrShift << " at " << i));
					}

				};
			}
			{
				string test_message = _MSGA("Divide function testing. Divide by sequential powers of 2 " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
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
			{
				string test_message = _MSGA("Divide function testing. Ran tests " << runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			};
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

			string runmsg = _MSGA("Divide function timing. Ran " << timingcount << " times.\n");
			Logger::WriteMessage(runmsg.c_str());
		};

		TEST_METHOD(ui512md_03_div_pnv)
		{
			// Path and non-volatile reg tests

			u64 seed = 0;
			_UI512(dividend) { 0 };
			_UI512(quotient) { 0 };
			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(remainder) { 0 };
			regs r_before{};
			regs r_after{};
			RandomFill(num1, &seed);
			//RandomFill(num2, &seed);
			set_uT64(num2, 3);
			for (int i = 0; i < runcount; i++)
			{
				reg_verify((u64*)&r_before);
				div_u(quotient, remainder, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			};
			{
				string test_message = _MSGA("Divide function: path and non-volatile reg tests. " << runcount << " times.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
		};
		TEST_METHOD(ui512md_04_div64)
		{
			u64 seed = 0;
			_UI512(dividend) { 0 };
			_UI512(quotient) { 0 };
			_UI512(expectedquotient) { 0 };

			u64 divisor = 0;
			u64 remainder = 0;
			u64 expectedremainder = 0;
			// Edge case tests
			// 1. zero divided by random
			zero_u(dividend);
			for (int i = 0; i < runcount; i++)
			{
				zero_u(dividend);
				divisor = RandomU64(&seed);
				zero_u(expectedquotient);
				expectedremainder = 0;
				div_uT64(quotient, &remainder, dividend, divisor);
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j],
						_MSGW(L"Quotient at word #" << j << " failed zero divided by random on run #" << i));
				};
				Assert::AreEqual(expectedremainder, remainder,
					_MSGW(L"Remainder failed zero divided by random on run #" << i));
			};
			// 2. random divided by one
			for (int i = 0; i < runcount; i++)
			{
				RandomFill(dividend, &seed);
				divisor = 1;
				copy_u(expectedquotient, dividend);
				expectedremainder = 0;
				div_uT64(quotient, &remainder, dividend, divisor);
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j],
						_MSGW(L"Quotient at word #" << j << " failed random divided by one on run #" << i));
				};
				Assert::AreEqual(expectedremainder, remainder,
					_MSGW(L"Remainder failed random divided by one " << i));
			};
			// 3. random divided by self
			for (int i = 0; i < runcount; i++)
			{
				zero_u(dividend);
				dividend[7] = RandomU64(&seed);
				divisor = dividend[7];
				set_uT64(expectedquotient, 1);
				expectedremainder = 0;
				div_uT64(quotient, &remainder, dividend, divisor);
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j],
						_MSGW(L"Quotient at word #" << j << " failed random divided by self on run #" << i));
				};
				Assert::AreEqual(expectedremainder, remainder,
					_MSGW(L"Remainder failed random divided by self " << i));
			};
			{
				string test_message = _MSGA("Divide (u64) function testing. Edge cases: zero divided by random, random divided by one, random divided by self. \n "
					<< runcount << " times each, with pseudo random values.\n";);
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			};

			// First test, a simple divide by two. 
			// Easy to check as the expected answer is a shift right,
			// and expected remainder is a shift left

			for (int i = 0; i < runcount; i++)
			{
				RandomFill(dividend, &seed);
				zero_u(quotient);
				divisor = 2;
				shr_u(expectedquotient, dividend, u16(1));
				expectedremainder = (dividend[7] << 63) >> 63;

				div_uT64(quotient, &remainder, dividend, divisor);

				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedquotient[j], quotient[j],
						_MSGW(L"Quotient at word #" << j << " failed on run #" << i));
				};

				Assert::AreEqual(expectedremainder, remainder, _MSGW(L"Remainder failed " << i));
			};
			{
				string test_message = _MSGA("Divide (u64) function testing. Simple divide by 2 "
					<< runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			};

			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for (u16 nrShift = 0; nrShift < 64; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < runcount / 64; i++)
				{
					RandomFill(dividend, &seed);
					divisor = 1ull << nrShift;
					shr_u(expectedquotient, dividend, nrShift);
					expectedremainder = (nrShift == 0) ? 0 : (dividend[7] << (64 - nrShift)) >> (64 - nrShift);

					div_uT64(quotient, &remainder, dividend, divisor);

					for (int j = 0; j < 8; j++)
					{
						Assert::AreEqual(expectedquotient[j], quotient[j],
							_MSGW(L"Quotient at word #" << j << " failed shifting: " << nrShift << " on run #" << i));
					}
					Assert::AreEqual(expectedremainder, remainder, _MSGW(L"Remainder failed shifting: " << nrShift << " on run #" << i));
				};
			}
			{
				string test_message = _MSGA("Divide function testing. Divide by sequential powers of 2 "
					<< runcount << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}

			// Third test, Use case tests, divide out to get decimal digits. Do whole with random,
			// and a knowable sample 
			{
				string digits = "";
				RandomFill(dividend, &seed);
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
			_UI512(dividend) { 0 };
			_UI512(quotient) { 0 };
			u64 divisor = 0;
			u64 remainder = 0;
			RandomFill(dividend, &seed);
			zero_u(quotient);
			divisor = RandomU64(&seed);

			for (int i = 0; i < timingcount; i++)
			{
				div_uT64(quotient, &remainder, dividend, divisor);
			};
			{
				string test_message = _MSGA("Divide by u64  function timing. Ran "
					<< timingcount << " times.\n");
				Logger::WriteMessage(test_message.c_str());
			};
		};

		TEST_METHOD(ui512md_04_div64_pnv)
		{
			// Path and non-volatile reg tests

			u64 seed = 0;
			_UI512(dividend) { 0 };
			_UI512(quotient) { 0 };
			_UI512(num1) { 0 };

			u64 num2{ 0 };
			u64 remainder{ 0 };
			regs r_before{};
			regs r_after{};
			RandomFill(num1, &seed);
			num2 = RandomU64(&seed);
			for (int i = 0; i < runcount; i++)
			{
				r_before.Clear();
				reg_verify((u64*)&r_before);
				div_uT64(quotient, &remainder, num1, num2);
				r_after.Clear();
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			};
			{
				string test_message = _MSGA("Divide by u64 function:  path and non-volatile reg tests. "
					<< runcount << " times.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
		};
	};
};
