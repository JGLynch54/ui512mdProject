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

		const s32 test_run_count = 1000;
		const s32 reg_verification_count = 5000;
		const s32 timing_count = 1000000;
		const s32 timing_count_short = 10000;
		const s32 timing_count_medium = 100000;
		const s32 timing_count_long = 1000000;

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
		/// <param name="var">512 bit variable to be filled</param>
		/// <param name="seed">seed for random number generator</param>
		/// <returns>none</returns>
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

			u64 seed = 0;
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("Multiply function testing.\n\nEdge cases:\n\tzero times zero,\n\tzero times random,\n\trandom times zero,\n\tone times random,\n\trandom times one.\n"
					<< test_run_count << " times each, with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n\n");
			};


			// Now the real tests, progressively more complex

			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left, expected overflow is a shift right
			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("First test, a simple multiply by two.  " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n\n");
			};

			// Second test, a simple multiply by a random power of two. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("Second test, a simple multiply by a random power of two. " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n\n");
			};

			// Third test, a simple multiply by a random 64 bit integer. 
			// Still relatively easy to check as the expected answer is a shift left,
			// expected overflow is a shift right
			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("Third test, a multiply by a random 64 bit value. " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n\n");
			};

			// Fourth test, a multiply by sums of random powers of two into a 64 bit value. 
			// Building "expected" is a bit more complicated. This test is more about correctly building "expected"
			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("Fourth test. Multiply by sums of random powers of two, building ""expected"" 64 bit only; " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: all via assert.\n\n");
			}

			// Fifth test, a multiply by sums of random powers of two into a 512 bit value. 
			// Building "expected" is a bit more complicated. 
			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("Fifth test. Multiply by sums of random powers of two, building ""expected"" full 512 bit; " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n\n");
			};
		};

		TEST_METHOD(ui512md_01_mul_performance_timing)
		{
			// Performance timing tests.
			// Ref: "Essentials of Modern Business Statistics", 7th Ed, by Anderson, Sweeney, Williams, Camm, Cochran. South-Western, 2015
			// Sections 3.2, 3.3, 3.4
			// Note: these tests are not pass/fail, they are informational only

			_UI512(num1) { 0 };
			_UI512(num2) { 0 };
			_UI512(product) { 0 };
			_UI512(overflow) { 0 };

			u64 seed = 0;

			double total_short = 0.0;
			double min_short = 1000000.0;
			double max_short = 0.0;
			double mean_short = 0.0;
			double sample_variance_short = 0.0;
			double stddev_short = 0.0;
			double coefficient_of_variation_short = 0.0;

			double total_medium = 0.0;
			double min_medium = 1000000.0;
			double max_medium = 0.0;
			double mean_medium = 0.0;
			double sample_variance_medium = 0.0;
			double stddev_medium = 0.0;
			double coefficient_of_variation_medium = 0.0;

			double total_long = 0.0;
			double min_long = 1000000.0;
			double max_long = 0.0;
			double mean_long = 0.0;
			double sample_variance_long = 0.0;
			double stddev_long = 0.0;
			double coefficient_of_variation_long = 0.0;

			double outlier_threshold = 0.0;
			struct outlier
			{
				int iteration;
				double duration;
				double variance;
				double z_score;
			};
			vector<outlier> outliers;

			// First batch, short run
			{
				std::vector<double> x_i_short(timing_count_short);
				std::vector<double> z_scores_short(timing_count_short);

				//Run target function timing_count_short times, capturing each time, also getting min, max, and total time spent
				for (int i = 0; i < timing_count_short; i++)
				{
					RandomFill(num1, &seed);
					RandomFill(num2, &seed);
					auto countStart = std::chrono::steady_clock::now();
					mult_u(product, overflow, num1, num2);
					auto countEnd = std::chrono::steady_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double duration = countDur.count();
					min_short = (duration < min_short) ? duration : min_short;
					max_short = (duration > max_short) ? duration : max_short;
					total_short += duration;
					x_i_short[i] = duration;
				};

				// Calculate mean, population variance, standard deviation, coefficient of variation, and z-scores
				{
					mean_short = total_short / double(timing_count_short);
					for (int i = 0; i < timing_count_short; i++)
					{
						double deviation = x_i_short[i] - mean_short;
						sample_variance_short += deviation * deviation;
					};

					sample_variance_short /= (double(timing_count_short) - 1.0);
					stddev_short = sqrt(sample_variance_short);
					coefficient_of_variation_short = (mean_short != 0.0) ? (stddev_short / mean_short) * 100.0 : 0.0;
					for (int i = 0; i < timing_count_short; i++)
					{
						z_scores_short[i] = (stddev_short != 0.0) ? (x_i_short[i] - mean_short) / stddev_short : 0.0;
					};

					string test_message = _MSGA("Multiply function performance timing test.\n\nFirst batch. \nRan for "
						<< timing_count_short << " samples.\nTotal target function (including c calling set-up) execution time: "
						<< total_short << " microseconds. \nAverage time per call: "
						<< mean_short << " microseconds.\nMinimum in "
						<< min_short << "\nMaximum in "
						<< max_short << "\n");

					test_message += _MSGA("Sample Variance: "
						<< sample_variance_short << " \nStandard Deviation: "
						<< stddev_short << "\nCoefficient of Variation: "
						<< coefficient_of_variation_short << "%\n\n");

					Logger::WriteMessage(test_message.c_str());
				};

				// Identify outliers, based on outlier_threshold
				for (int i = 0; i < timing_count_short; i++)
				{
					double z_sc = z_scores_short[i];
					double abs_z_score = (z_sc < 0.0) ? -z_sc : z_sc;
					outlier_threshold = 3.0 * stddev_short;
					if (abs_z_score > 3.0)
					{
						outlier o;
						o.iteration = i;
						o.duration = x_i_short[i];
						o.z_score = z_sc;
						outliers.push_back(o);
					};
				};

				// Report on outliers, if any
				// Note: in a normal distribution, 99.7% of all values will be within three standard deviations of the mean
				// Thus, any value outside that range is an outlier
				// In this test, we are looking for unexpected occurrences of outliers, which may indicate some kind of
				// external interference in the timing test (such as OS activity, etc)
				// If the number of outliers is small (say under 1% of total), then it is likely not a problem
				// If the number of outliers is larger (say over 5% of total), then there may be a problem with the test environment

				// Note: some functions may have a bi-modal distribution, in which case you must temper the results of the outlier test.
				// mult_u has a propagating carry loop, which may cause a bi-modal distribution, or at least a widening of the distribution
				// Further, the CPU clock penalty of starting AVX instructions on some processors may cause outliers
				// If we have a significant number of outliers, then we have a problem
				double outlier_percentage = (double)(outliers.size() * 100.0) / (double)timing_count_short;
				double range_low = mean_short - (3.0 * stddev_short);
				double range_high = mean_short + (3.0 * stddev_short);
				range_low = (range_low < 0.0) ? 0.0 : range_low;

				if (outliers.size() > 0)
				{
					string test_message = _MSGA("Identified " << outliers.size() << " outlier(s), based on a threshold of "
						<< outlier_threshold << " which is three standard deviations from the mean of " << mean_short << " microseconds (us).\n");
					test_message += _MSGA("Samples with execution times from " << range_low << " us to " << range_high << " us, are within that range.\n");
					test_message += format("Samples within this range are considered normal and contain {:6.3f}% of the samples.\n", (100.0 - outlier_percentage));
					test_message += "Samples outside this range are considered outliers. ";
					test_message += format("This represents {:6.3f}% of the samples.", outlier_percentage);
					test_message += "\nTested via Assert that the percentage of outliers is below 1%\n";
					test_message += "\nUp to the first 20 are shown. z_score is the number of standards of deviation the outlier varies from the mean.\n\n";
					test_message += " Iteration | Duration (us) | Z Score (us)  | \n";
					test_message += "-----------|---------------|---------------|\n";
					s32 outlier_limit = 20;
					s32 count = 0;
					for (auto& o : outliers) {
						if (count++ >= outlier_limit) break;
						test_message += format("{:10d} |", o.iteration);
						test_message += format("{:13.2f}  |", o.duration);
						test_message += format("{:13.4f}  |", o.z_score);
						test_message += "\n";
					}
					test_message += "\n";
					Assert::IsTrue(outlier_percentage < 1.0, L"Too many outliers, over 1% of total sample");
					Logger::WriteMessage(test_message.c_str());
				};

				// End of first batch
				x_i_short.clear();
				z_scores_short.clear();
				outliers.clear();
			};

			// Second batch, medium run
			{
				std::vector<double> x_i_medium(timing_count_medium);
				std::vector<double> z_scores_medium(timing_count_medium);
				outliers.clear();

				//Run target function timing_count_medium times, capturing each time, also getting min, max, and total time spent
				for (int i = 0; i < timing_count_medium; i++)
				{
					RandomFill(num1, &seed);
					RandomFill(num2, &seed);
					auto countStart = std::chrono::steady_clock::now();
					mult_u(product, overflow, num1, num2);
					auto countEnd = std::chrono::steady_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double duration = countDur.count();
					min_medium = (duration < min_medium) ? duration : min_medium;
					max_medium = (duration > max_medium) ? duration : max_medium;
					total_medium += duration;
					x_i_medium[i] = duration;
				};

				// Calculate mean, population variance, standard deviation, coefficient of variation, and z-scores
				{
					mean_medium = total_medium / double(timing_count_medium);
					for (int i = 0; i < timing_count_medium; i++)
					{
						double deviation = x_i_medium[i] - mean_medium;
						sample_variance_medium += deviation * deviation;
					};
					sample_variance_medium /= (double(timing_count_medium - 1.0));
					stddev_medium = sqrt(sample_variance_medium);
					coefficient_of_variation_medium = (mean_medium != 0.0) ? (stddev_medium / mean_medium) * 100.0 : 0.0;
					for (int i = 0; i < timing_count_medium; i++)
					{
						z_scores_medium[i] = (stddev_medium != 0.0) ? (x_i_medium[i] - mean_medium) / stddev_medium : 0.0;
					};

					string test_message = _MSGA("\nSecond batch.\nRan for "
						<< timing_count_medium << " samples.\nTotal target function (including c calling set-up) execution time: "
						<< total_medium << " microseconds.\nAverage time per call: "
						<< mean_medium << " microseconds.\nMinimum in "
						<< min_medium << "\nMaximum in "
						<< max_medium << "\n");

					test_message += _MSGA("Sample Variance: "
						<< sample_variance_medium << "\nStandard Deviation: "
						<< stddev_medium << "\nCoefficient of Variation: "
						<< coefficient_of_variation_medium << "%\n\n");

					Logger::WriteMessage(test_message.c_str());
				};

				// Identify outliers, based on outlier_threshold
				for (int i = 0; i < timing_count_medium; i++)
				{
					double z_sc = z_scores_medium[i];
					double abs_z_score = (z_sc < 0.0) ? -z_sc : z_sc;
					outlier_threshold = 3.0 * stddev_medium;
					if (abs_z_score > 3.0)
					{
						outlier o;
						o.iteration = i;
						o.duration = x_i_medium[i];
						o.z_score = z_sc;
						outliers.push_back(o);
					};
				};

				// Report on outliers, if any
				double outlier_percentage = (double)(outliers.size() * 100.0) / (double)timing_count_medium;
				double range_low = mean_medium - (3.0 * stddev_medium);
				double range_high = mean_medium + (3.0 * stddev_medium);
				range_low = (range_low < 0.0) ? 0.0 : range_low;
				if (outliers.size() > 0)
				{
					string test_message = _MSGA("Identified " << outliers.size() << " outlier(s), based on a threshold of "
						<< outlier_threshold << " which is three standard deviations from the mean of " << mean_medium << " microseconds (us).\n");
					test_message += _MSGA("Samples with execution times from " << range_low << " us to " << range_high << " us, are within that range.\n");
					test_message += format("Samples within this range are considered normal and contain {:6.3f}% of the samples.\n", (100.0 - outlier_percentage));
					test_message += "Samples outside this range are considered outliers. ";
					test_message += format("This represents {:6.3f}% of the samples.", outlier_percentage);
					test_message += "\nTested via Assert that the percentage of outliers is below 1%\n";
					test_message += "\nUp to the first 20 are shown. z_score is the number of standards of deviation the outlier varies from the mean.\n\n";
					test_message += " Iteration | Duration (us) | Z Score (us)  | \n";
					test_message += "-----------|---------------|---------------|\n";
					s32 outlier_limit = 20;
					s32 count = 0;
					for (auto& o : outliers) {
						if (count++ >= outlier_limit) break;
						test_message += format("{:10d} |", o.iteration);
						test_message += format("{:13.2f}  |", o.duration);
						test_message += format("{:13.4f}  |", o.z_score);
						test_message += "\n";
					}
					test_message += "\n";
					Assert::IsTrue(outlier_percentage < 1.0, L"Too many outliers, over 1% of total sample");
					Logger::WriteMessage(test_message.c_str());
				};

				// End of second batch
				x_i_medium.clear();
				z_scores_medium.clear();
			};
			// Third batch, long run
			{
				std::vector<double> x_i_long(timing_count_long);
				std::vector<double> z_scores_long(timing_count_long);
				outliers.clear();
				//Run target function timing_count_long times, capturing each time, also getting min, max, and total time spent
				for (int i = 0; i < timing_count_long; i++)
				{
					RandomFill(num1, &seed);
					RandomFill(num2, &seed);
					auto countStart = std::chrono::steady_clock::now();
					mult_u(product, overflow, num1, num2);
					auto countEnd = std::chrono::steady_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double duration = countDur.count();
					min_long = (duration < min_long) ? duration : min_long;
					max_long = (duration > max_long) ? duration : max_long;
					total_long += duration;
					x_i_long[i] = duration;
				};
				// Calculate mean, population variance, standard deviation, coefficient of variation, and z-scores
				{
					mean_long = total_long / double(timing_count_long);
					for (int i = 0; i < timing_count_long; i++)
					{
						double deviation = x_i_long[i] - mean_long;
						sample_variance_long += deviation * deviation;
					};
					sample_variance_long /= (double(timing_count_long) - 1.0);
					stddev_long = sqrt(sample_variance_long);
					coefficient_of_variation_long = (mean_long != 0.0) ? (stddev_long / mean_long) * 100.0 : 0.0;
					for (int i = 0; i < timing_count_long; i++)
					{
						z_scores_long[i] = (stddev_long != 0.0) ? (x_i_long[i] - mean_long) / stddev_long : 0.0;
					};
					string test_message = _MSGA("\nThird batch. \nRan for "
						<< timing_count_long << " samples.\nTotal target function (including c calling set-up) execution time: "
						<< total_long << " microseconds. \nAverage time per call : "
						<< mean_long << " microseconds.\nMinimum in "
						<< min_long << "\nMaximum in "
						<< max_long << "\n");

					test_message += _MSGA("Sample Variance: "
						<< sample_variance_long << "\nStandard Deviation: "
						<< stddev_long << "\nCoefficient of Variation: "
						<< coefficient_of_variation_long << "%\n\n");

					Logger::WriteMessage(test_message.c_str());
				};

				// Identify outliers, based on outlier_threshold
				for (int i = 0; i < timing_count_long; i++)
				{
					double z_sc = z_scores_long[i];
					double abs_z_score = (z_sc < 0.0) ? -z_sc : z_sc;
					outlier_threshold = 3.0 * stddev_long;
					if (abs_z_score > 3.0)
					{
						outlier o;
						o.iteration = i;
						o.duration = x_i_long[i];
						o.z_score = z_sc;
						outliers.push_back(o);
					};
				};

				// Report on outliers, if any
				double outlier_percentage = (double)(outliers.size() * 100.0) / (double)timing_count_long;
				double range_low = mean_long - (3.0 * stddev_long);
				double range_high = mean_long + (3.0 * stddev_long);
				range_low = (range_low < 0.0) ? 0.0 : range_low;
				if (outliers.size() > 0)
				{
					string test_message = _MSGA("Identified " << outliers.size() << " outlier(s), based on a threshold of "
						<< outlier_threshold << " which is three standard deviations from the mean of " << mean_long << " microseconds (us).\n");
					test_message += _MSGA("Samples with execution times from " << range_low << " us to " << range_high << " us, are within that range.\n");
					test_message += format("Samples within this range are considered normal and contain {:6.3f}% of the samples.\n", (100.0 - outlier_percentage));
					test_message += "Samples outside this range are considered outliers. ";
					test_message += format("This represents {:6.3f}% of the samples.", outlier_percentage);
					test_message += "\nTested via Assert that the percentage of outliers is below 1%\n";
					test_message += "\nUp to the first 20 are shown. z_score is the number of standards of deviation the outlier varies from the mean.\n\n";
					test_message += " Iteration | Duration (us) | Z Score (us)  | \n";
					test_message += "-----------|---------------|---------------|\n";
					s32 outlier_limit = 20;
					s32 count = 0;
					for (auto& o : outliers) {
						if (count++ >= outlier_limit) break;
						test_message += format("{:10d} |", o.iteration);
						test_message += format("{:13.2f}  |", o.duration);
						test_message += format("{:13.4f}  |", o.z_score);
						test_message += "\n";
					}
					Logger::WriteMessage(test_message.c_str());
				};

				// End of third batch
				x_i_long.clear();
				z_scores_long.clear();
				outliers.clear();
			};
		};

		TEST_METHOD(ui512md_02_mul64)
		{
			// mult_uT64 tests
			// multistage testing, part for use as debugging, progressively "real" testing
			// Note: the ui512a module must pass testing before these tests, as adds are used in this test
			// Note: the ui512b module must pass testing before these tests, as 'or' and shifts are used in this test

			u64 seed = 0;
			regs r_before{};
			regs r_after{};
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
			zero_u(expectedproduct);
			num2 = 0;
			expectedoverflow = 0;
			reg_verify((u64*)&r_before);
			s16 ret = mult_uT64(product, &overflow, num1, num2);
			reg_verify((u64*)&r_after);
			Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			Assert::AreEqual(s16(0), ret, L"Return code failed zero times zero test.");
			Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed zero times zero"));
			for (int j = 0; j < 8; j++)
			{
				Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed zero times zero"));
			};

			// 2. zero times random
			for (int i = 0; i < test_run_count; i++)
			{
				zero_u(num1);
				zero_u(expectedproduct);
				num2 = RandomU64(&seed);
				expectedoverflow = 0;
				reg_verify((u64*)&r_before);
				s16 ret = mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed zero times random test.");
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow  failed zero times random " << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed zero times random on run #" << i));
				};
			};

			// 3. random times zero
			for (int i = 0; i < test_run_count; i++)
			{
				num2 = 0;
				RandomFill(num1, &seed);
				zero_u(expectedproduct);
				expectedoverflow = 0;
				reg_verify((u64*)&r_before);
				s16 ret = mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random times zero test.");
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed random times zero " << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed random times zero on run #" << i));
				};
			};

			// 4. one times random
			for (int i = 0; i < test_run_count; i++)
			{
				set_uT64(num1, 1ull);
				num2 = RandomU64(&seed);
				set_uT64(expectedproduct, num2);
				expectedoverflow = 0;
				reg_verify((u64*)&r_before);
				s16 ret = mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed one times random test.");
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed one times random on run #" << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed one times random on run #" << i));
				};
			};

			// 5. random times one
			for (int i = 0; i < test_run_count; i++)
			{
				num2 = 1ull;
				RandomFill(num1, &seed);
				copy_u(expectedproduct, num1);
				expectedoverflow = 0;
				reg_verify((u64*)&r_before);
				s16 ret = mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random times one test.");
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed random times one on run #" << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed random times one on run #" << i));
				};
			};

			{
				string test_message = _MSGA("Multiply (x64) function testing.\n\nEdge cases:\n\tzero times zero,\n\tzero times random,\n\trandom times zero,\n\tone times random,\n\trandom times one.\n"
					<< test_run_count << " times each, with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n\n");
			};

			// Now the real tests, progressively more complex
			// First test, a simple multiply by two. 
			// Easy to check as the expected answer is a shift left,
			// and expected overflow is a shift right
			for (int i = 0; i < test_run_count; i++)
			{
				for (int j = 0; j < 8; j++)
				{
					num1[j] = RandomU64(&seed);
				};

				num2 = 2;
				shl_u(expectedproduct, num1, u16(1));
				expectedoverflow = num1[0] >> 63;
				reg_verify((u64*)&r_before);
				s16 ret = mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random times one test.");
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed " << i));
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on ru #" << i));
				};
			};

			string runmsg1 = "Multiply (u64) function testing. First test. Simple multiply by 2 " +
				to_string(test_run_count) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage(runmsg1.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n\n");

			// Second test, a simple multiply by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift left,
			// and expected overflow is a shift right
			for (u16 nrShift = 0; nrShift < 64; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < test_run_count / 64; i++)
				{
					RandomFill(num1, &seed);
					num2 = 1ull << nrShift;
					shl_u(expectedproduct, num1, nrShift);
					expectedoverflow = (nrShift == 0) ? 0 : num1[0] >> (64 - nrShift);
					reg_verify((u64*)&r_before);
					s16 ret = mult_uT64(product, &overflow, num1, num2);
					reg_verify((u64*)&r_after);
					Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
					Assert::AreEqual(s16(0), ret, L"Return code failed random times one test.");
					for (int j = 0; j < 8; j++)
					{
						Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed shift: " << nrShift << " on run #" << i));
					}
					Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed shift: " << nrShift << " on run #" << i));
				};
			}

			string runmsg2 = "Multiply (u64) function testing. Second test. Multiply by sequential powers of 2 "
				+ to_string(test_run_count) + " times, each with pseudo random values.\n";
			Logger::WriteMessage(runmsg2.c_str());
			Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n\n");

			// Third test, a multiply by random sums of powers of two. 
			// Building "expected" is a bit more complicated
			const u16 nrBits = 24;
			u16 BitsUsed[nrBits] = { 0 };
			for (int i = 0; i < test_run_count; i++)
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

				reg_verify((u64*)&r_before);
				s16 ret = mult_uT64(product, &overflow, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
				Assert::AreEqual(s16(0), ret, L"Return code failed random times one test.");
				// Now compare results
				for (int j = 0; j < 8; j++)
				{
					Assert::AreEqual(expectedproduct[j], product[j], _MSGW(L"Product at word #" << j << " failed on run #" << i << " num2: " << num2));
				}
				Assert::AreEqual(expectedoverflow, overflow, _MSGW(L"Overflow failed " << i));
			};
			{
				string test_message = _MSGA("Multiply (x64) function testing. Third test. Multiply by sums of random powers of two, building ""expected""; "
					<< test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values, return value, and volatile register integrity: each via assert.\n\n");
			}
		};
		TEST_METHOD(ui512md_02_mul64_performance_timing)
		{
			// Performance timing tests.
			// Ref: "Essentials of Modern Business Statistics", 7th Ed, by Anderson, Sweeney, Williams, Camm, Cochran. South-Western, 2015
			// Sections 3.2, 3.3, 3.4
			// Note: these tests are not pass/fail, they are informational only

			_UI512(num1) { 0 };
			_UI512(product) { 0 };
			u64 overflow = 0;
			u64 num2 = 0;
			u64 seed = 0;

			double total_short = 0.0;
			double min_short = 1000000.0;
			double max_short = 0.0;
			double mean_short = 0.0;
			double sample_variance_short = 0.0;
			double stddev_short = 0.0;
			double coefficient_of_variation_short = 0.0;

			double total_medium = 0.0;
			double min_medium = 1000000.0;
			double max_medium = 0.0;
			double mean_medium = 0.0;
			double sample_variance_medium = 0.0;
			double stddev_medium = 0.0;
			double coefficient_of_variation_medium = 0.0;

			double total_long = 0.0;
			double min_long = 1000000.0;
			double max_long = 0.0;
			double mean_long = 0.0;
			double sample_variance_long = 0.0;
			double stddev_long = 0.0;
			double coefficient_of_variation_long = 0.0;

			double outlier_threshold = 0.0;

			struct outlier
			{
				int iteration;
				double duration;
				double variance;
				double z_score;
			};
			vector<outlier> outliers;

			// First batch, short run
			{
				std::vector<double> x_i_short(timing_count_short);
				std::vector<double> z_scores_short(timing_count_short);

				//Run target function timing_count_short times, capturing each time, also getting min, max, and total time spent
				for (int i = 0; i < timing_count_short; i++)
				{
					RandomFill(num1, &seed);
					num2 = RandomU64(&seed);
					auto countStart = std::chrono::steady_clock::now();
					mult_uT64(product, &overflow, num1, num2);
					auto countEnd = std::chrono::steady_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double duration = countDur.count();
					min_short = (duration < min_short) ? duration : min_short;
					max_short = (duration > max_short) ? duration : max_short;
					total_short += duration;
					x_i_short[i] = duration;
				};

				// Calculate mean, population variance, standard deviation, coefficient of variation, and z-scores
				{
					mean_short = total_short / double(timing_count_short);
					for (int i = 0; i < timing_count_short; i++)
					{
						double deviation = x_i_short[i] - mean_short;
						sample_variance_short += deviation * deviation;
					};

					sample_variance_short /= (double(timing_count_short) - 1.0);
					stddev_short = sqrt(sample_variance_short);
					coefficient_of_variation_short = (mean_short != 0.0) ? (stddev_short / mean_short) * 100.0 : 0.0;
					for (int i = 0; i < timing_count_short; i++)
					{
						z_scores_short[i] = (stddev_short != 0.0) ? (x_i_short[i] - mean_short) / stddev_short : 0.0;
					};

					string test_message = _MSGA("Multiply (x64) function performance timing test.\nFirst batch. \nRan for "
						<< timing_count_short << " samples.\nTotal target function (including c calling set-up) execution time: "
						<< total_short << " microseconds. \nAverage time per call : "
						<< mean_short << " microseconds.\nMinimum in "
						<< min_short << "\nMaximum in "
						<< max_short << "\n");

					test_message += _MSGA("Sample Variance: "
						<< sample_variance_short << "\nStandard Deviation: "
						<< stddev_short << "\nCoefficient of Variation: "
						<< coefficient_of_variation_short << "%\n\n");

					Logger::WriteMessage(test_message.c_str());
				};

				// Identify outliers, based on outlier_threshold
				for (int i = 0; i < timing_count_short; i++)
				{
					double z_sc = z_scores_short[i];
					double abs_z_score = (z_sc < 0.0) ? -z_sc : z_sc;
					outlier_threshold = 3.0 * stddev_short;
					if (abs_z_score > 3.0)
					{
						outlier o;
						o.iteration = i;
						o.duration = x_i_short[i];
						o.z_score = z_sc;
						outliers.push_back(o);
					};
				};

				double outlier_percentage = (double)(outliers.size() * 100.0) / (double)timing_count_short;
				double range_low = mean_short - (3.0 * stddev_short);
				double range_high = mean_short + (3.0 * stddev_short);
				range_low = (range_low < 0.0) ? 0.0 : range_low;
				if (outliers.size() > 0)
				{
					string test_message = _MSGA("Identified " << outliers.size() << " outlier(s), based on a threshold of "
						<< outlier_threshold << " which is three standard deviations from the mean of " << mean_short << " microseconds (us).\n");
					test_message += _MSGA("Samples with execution times from " << range_low << " us to " << range_high << " us, are within that range.\n");
					test_message += format("Samples within this range are considered normal and contain {:6.3f}% of the samples.\n", (100.0 - outlier_percentage));
					test_message += "Samples outside this range are considered outliers. ";
					test_message += format("This represents {:6.3f}% of the samples.", outlier_percentage);
					test_message += "\nTested via Assert that the percentage of outliers is below 1%\n";
					test_message += "\nUp to the first 20 are shown. z_score is the number of standards of deviation the outlier varies from the mean.\n\n";
					test_message += " Iteration | Duration (us) | Z Score (us)  | \n";
					test_message += "-----------|---------------|---------------|\n";
					s32 outlier_limit = 20;
					s32 count = 0;
					for (auto& o : outliers) {
						if (count++ >= outlier_limit) break;
						test_message += format("{:10d} |", o.iteration);
						test_message += format("{:13.2f}  |", o.duration);
						test_message += format("{:13.4f}  |", o.z_score);
						test_message += "\n";
					}
					test_message += "\n";
					Assert::IsTrue(outlier_percentage < 1.0, L"Too many outliers, over 1% of total sample");
					Logger::WriteMessage(test_message.c_str());
				};

				// End of first batch
				x_i_short.clear();
				z_scores_short.clear();
				outliers.clear();
			};

			// Second batch, medium run
			{
				std::vector<double> x_i_medium(timing_count_medium);
				std::vector<double> z_scores_medium(timing_count_medium);
				outliers.clear();

				//Run target function timing_count_medium times, capturing each time, also getting min, max, and total time spent
				for (int i = 0; i < timing_count_medium; i++)
				{
					RandomFill(num1, &seed);
					num2 = RandomU64(&seed);
					auto countStart = std::chrono::steady_clock::now();
					mult_uT64(product, &overflow, num1, num2);
					auto countEnd = std::chrono::steady_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double duration = countDur.count();
					min_medium = (duration < min_medium) ? duration : min_medium;
					max_medium = (duration > max_medium) ? duration : max_medium;
					total_medium += duration;
					x_i_medium[i] = duration;
				};

				// Calculate mean, population variance, standard deviation, coefficient of variation, and z-scores
				{
					mean_medium = total_medium / double(timing_count_medium);
					for (int i = 0; i < timing_count_medium; i++)
					{
						double deviation = x_i_medium[i] - mean_medium;
						sample_variance_medium += deviation * deviation;
					};
					sample_variance_medium /= (double(timing_count_medium) - 1.0);
					stddev_medium = sqrt(sample_variance_medium);
					coefficient_of_variation_medium = (mean_medium != 0.0) ? (stddev_medium / mean_medium) * 100.0 : 0.0;
					for (int i = 0; i < timing_count_medium; i++)
					{
						z_scores_medium[i] = (stddev_medium != 0.0) ? (x_i_medium[i] - mean_medium) / stddev_medium : 0.0;
					};

					string test_message = _MSGA("\nSecond batch. \nRan for "
						<< timing_count_medium << " samples.\nTotal target function (including c calling set-up) execution time: "
						<< total_medium << " microseconds. \nAverage time per call : "
						<< mean_medium << " microseconds.\nMinimum in "
						<< min_medium << " \nMaximum in "
						<< max_medium << " \n");

					test_message += _MSGA("Sample Variance: "
						<< sample_variance_medium << "\nStandard Deviation: "
						<< stddev_medium << "\nCoefficient of Variation: "
						<< coefficient_of_variation_medium << "%\n\n");

					Logger::WriteMessage(test_message.c_str());
				};

				// Identify outliers, based on outlier_threshold
				for (int i = 0; i < timing_count_medium; i++)
				{
					double z_sc = z_scores_medium[i];
					double abs_z_score = (z_sc < 0.0) ? -z_sc : z_sc;
					outlier_threshold = 3.0 * stddev_medium;
					if (abs_z_score > 3.0)
					{
						outlier o;
						o.iteration = i;
						o.duration = x_i_medium[i];
						o.z_score = z_sc;
						outliers.push_back(o);
					};
				};

				// Report on outliers, if any
				double outlier_percentage = (double)(outliers.size() * 100.0) / (double)timing_count_medium;
				double range_low = mean_medium - (3.0 * stddev_medium);
				double range_high = mean_medium + (3.0 * stddev_medium);
				range_low = (range_low < 0.0) ? 0.0 : range_low;

				if (outliers.size() > 0)
				{
					string test_message = _MSGA("Identified " << outliers.size() << " outlier(s), based on a threshold of "
						<< outlier_threshold << " which is three standard deviations from the mean of " << mean_medium << " microseconds (us).\n");
					test_message += _MSGA("Samples with execution times from " << range_low << " us to " << range_high << " us, are within that range.\n");
					test_message += format("Samples within this range are considered normal and contain {:6.3f}% of the samples.\n", (100.0 - outlier_percentage));
					test_message += "Samples outside this range are considered outliers. ";
					test_message += format("This represents {:6.3f}% of the samples.", outlier_percentage);
					test_message += "\nTested via Assert that the percentage of outliers is below 1%\n";
					test_message += "\nUp to the first 20 are shown. z_score is the number of standards of deviation the outlier varies from the mean.\n\n";
					test_message += " Iteration | Duration (us) | Z Score (us)  | \n";
					test_message += "-----------|---------------|---------------|\n";
					s32 outlier_limit = 20;
					s32 count = 0;
					for (auto& o : outliers) {
						if (count++ >= outlier_limit) break;
						test_message += format("{:10d} |", o.iteration);
						test_message += format("{:13.2f}  |", o.duration);
						test_message += format("{:13.4f}  |", o.z_score);
						test_message += "\n";
					}
					test_message += "\n";
					Assert::IsTrue(outlier_percentage < 1.0, L"Too many outliers, over 1% of total sample");
					Logger::WriteMessage(test_message.c_str());
				};

				// End of second batch
				x_i_medium.clear();
				z_scores_medium.clear();
			};
			// Third batch, long run
			{
				std::vector<double> x_i_long(timing_count_long);
				std::vector<double> z_scores_long(timing_count_long);
				outliers.clear();
				//Run target function timing_count_long times, capturing each time, also getting min, max, and total time spent
				for (int i = 0; i < timing_count_long; i++)
				{
					RandomFill(num1, &seed);
					num2 = RandomU64(&seed);
					auto countStart = std::chrono::steady_clock::now();
					mult_uT64(product, &overflow, num1, num2);
					auto countEnd = std::chrono::steady_clock::now();
					std::chrono::duration<double, std::micro> countDur = countEnd - countStart;
					double duration = countDur.count();
					min_long = (duration < min_long) ? duration : min_long;
					max_long = (duration > max_long) ? duration : max_long;
					total_long += duration;
					x_i_long[i] = duration;
				};
				// Calculate mean, population variance, standard deviation, coefficient of variation, and z-scores
				{
					mean_long = total_long / double(timing_count_long);
					for (int i = 0; i < timing_count_long; i++)
					{
						double deviation = x_i_long[i] - mean_long;
						sample_variance_long += deviation * deviation;
					};
					sample_variance_long /= (double(timing_count_long) - 1.0);
					stddev_long = sqrt(sample_variance_long);
					coefficient_of_variation_long = (mean_long != 0.0) ? (stddev_long / mean_long) * 100.0 : 0.0;
					for (int i = 0; i < timing_count_long; i++)
					{
						z_scores_long[i] = (stddev_long != 0.0) ? (x_i_long[i] - mean_long) / stddev_long : 0.0;
					};
					string test_message = _MSGA("\nThird batch.\nRan for "
						<< timing_count_long << " samples.\nTotal target function (including c calling set-up) execution time: "
						<< total_long << " microseconds. \nAverage time per call : "
						<< mean_long << " microseconds.\nMinimum in "
						<< min_long << "\nMaximum in "
						<< max_long << "\n");

					test_message += _MSGA("Sample Variance: "
						<< sample_variance_long << "\nStandard Deviation: "
						<< stddev_long << "\nCoefficient of Variation: "
						<< coefficient_of_variation_long << "%\n\n");

					Logger::WriteMessage(test_message.c_str());
				};

				// Identify outliers, based on outlier_threshold
				for (int i = 0; i < timing_count_long; i++)
				{
					double z_sc = z_scores_long[i];
					double abs_z_score = (z_sc < 0.0) ? -z_sc : z_sc;
					outlier_threshold = 3.0 * stddev_long;
					if (abs_z_score > 3.0)
					{
						outlier o;
						o.iteration = i;
						o.duration = x_i_long[i];
						o.z_score = z_sc;
						outliers.push_back(o);
					};
				};

				// Report on outliers, if any
				double outlier_percentage = (double)(outliers.size() * 100.0) / (double)timing_count_long;
				double range_low = mean_long - (3.0 * stddev_long);
				double range_high = mean_long + (3.0 * stddev_long);
				range_low = (range_low < 0.0) ? 0.0 : range_low;
				if (outliers.size() > 0)
				{
					string test_message = _MSGA("Identified " << outliers.size() << " outlier(s), based on a threshold of "
						<< outlier_threshold << " which is three standard deviations from the mean of " << mean_long << " microseconds (us).\n");
					test_message += _MSGA("Samples with execution times from " << range_low << " us to " << range_high << " us, are within that range.\n");
					test_message += format("Samples within this range are considered normal and contain {:6.3f}% of the samples.\n", (100.0 - outlier_percentage));
					test_message += "Samples outside this range are considered outliers. ";
					test_message += format("This represents {:6.3f}% of the samples.", outlier_percentage);
					test_message += "\nTested via Assert that the percentage of outliers is below 1%\n";
					test_message += "\nUp to the first 20 are shown. z_score is the number of standards of deviation the outlier varies from the mean.\n\n";
					test_message += " Iteration | Duration (us) | Z Score (us)  | \n";
					test_message += "-----------|---------------|---------------|\n";
					s32 outlier_limit = 20;
					s32 count = 0;
					for (auto& o : outliers) {
						if (count++ >= outlier_limit) break;
						test_message += format("{:10d} |", o.iteration);
						test_message += format("{:13.2f}  |", o.duration);
						test_message += format("{:13.4f}  |", o.z_score);
						test_message += "\n";
					}
					test_message += "\n";
					Assert::IsTrue(outlier_percentage < 1.0, L"Too many outliers, over 1% of total sample");
					Logger::WriteMessage(test_message.c_str());
				};

				// End of third batch
				x_i_long.clear();
				z_scores_long.clear();
				outliers.clear();
			};
		}
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
					<< test_run_count << " times each, with pseudo random values. Non-volatile registers verified.\n");
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

			for (int i = 0; i < test_run_count; i++)
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
				string test_message = _MSGA("Divide function testing. Simple divide by 2 " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for (u16 nrShift = 0; nrShift < 512; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < test_run_count / 512; i++)
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
				string test_message = _MSGA("Divide function testing. Divide by sequential powers of 2 " << test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
			//	Use case testing
			//		Divide number by common use case examples

			int adjruncount = test_run_count / 64;
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
				string test_message = _MSGA("Divide function testing. Ran tests " << test_run_count << " times, each with pseudo random values.\n");
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

			for (int i = 0; i < timing_count; i++)
			{
				div_u(quotient, remainder, dividend, divisor);
			};

			string runmsg = _MSGA("Divide function timing. Ran " << timing_count << " times.\n");
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
			for (int i = 0; i < test_run_count; i++)
			{
				reg_verify((u64*)&r_before);
				div_u(quotient, remainder, num1, num2);
				reg_verify((u64*)&r_after);
				Assert::IsTrue(r_before.AreEqual(&r_after), L"Register validation failed");
			};
			{
				string test_message = _MSGA("Divide function: path and non-volatile reg tests. " << test_run_count << " times.\n");
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
			for (int i = 0; i < test_run_count; i++)
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
					<< test_run_count << " times each, with pseudo random values.\n";);
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			};

			// First test, a simple divide by two. 
			// Easy to check as the expected answer is a shift right,
			// and expected remainder is a shift left

			for (int i = 0; i < test_run_count; i++)
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
					<< test_run_count << " times, each with pseudo random values.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			};

			// Second test, a simple divide by sequential powers of two. 
			// Still relatively easy to check as expected answer is a shift right,
			// and expected remainder is a shift left

			for (u16 nrShift = 0; nrShift < 64; nrShift++)	// rather than a random bit, cycle thru all 64 bits 
			{
				for (int i = 0; i < test_run_count / 64; i++)
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
					<< test_run_count << " times, each with pseudo random values.\n");
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

			for (int i = 0; i < timing_count; i++)
			{
				div_uT64(quotient, &remainder, dividend, divisor);
			};
			{
				string test_message = _MSGA("Divide by u64  function timing. Ran "
					<< timing_count << " times.\n");
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
			for (int i = 0; i < test_run_count; i++)
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
					<< test_run_count << " times.\n");
				Logger::WriteMessage(test_message.c_str());
				Logger::WriteMessage(L"Passed. Tested expected values via assert.\n\n");
			}
		};
	};
};
