#include "pch.h"
#include "CppUnitTest.h"
#include "ui512md.h"

using namespace std;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ui512mdTests
{
	TEST_CLASS ( ui512mdTests )
	{
	public:

		const s32 runcount = 1000;
		const s32 timingcount = 1000000;

		/// <summary>
		/// Random nnumber generator
		/// uses linear congruential method 
		/// ref: Knuth, Art Of Computer Programming, Vol. 2, Seminumerical Algorithms, 3rd Ed. Sec 3.2.1
		/// </summary>
		/// <param name="seed">if zero will supply with: 4294967291</param>
		/// <returns>Pseudo-random number from zero to ~2^63 (9223372036854775807)</returns>
		u64 RandomU64 ( u64* seed )
		{
			const u64 m = 9223372036854775807ull;			// 2^63 - 1, a Mersenne prime
			const u64 a = 68719476721ull;					// closest prime below 2^36
			const u64 c = 268435399ull;						// closest prime below 2^28
			// suggested seed: around 2^32, 4294967291
			// linear congruential method (ref: Knuth, Art Of Computer Programming, Vol. 2, Seminumerical Algorithms, 3rd Ed. Sec 3.2.1
			*seed = ( *seed == 0ull ) ? ( a * 4294967291ull + c ) % m : ( a * *seed + c ) % m;
			return *seed;
		};

		TEST_METHOD ( random_number_generator )
		{
			u64 seed = 0;
			//u64* seedptr = &seed;
			u32 dist [ 10 ] { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			const u64 split = 9223372036854775807ull / 10;
			u32 distc = 0;
			const u32 randomcount = 100000;

			for ( u32 i = 0; i < randomcount; i++ )
			{
				seed = RandomU64 ( &seed );
				dist [ u64 ( seed / split ) ]++;
			};

			string msgd = "\nDistribution of ( " + to_string ( randomcount ) + " ) pseudo-random numbers by deciles:\n";
			for ( int i = 0; i < 10; i++ )
			{
				msgd += to_string ( dist [ i ] ) + " ";
				distc += dist [ i ];
			};

			msgd += " summing to " + to_string ( distc ) + "\n";
			Logger::WriteMessage ( msgd.c_str ( ) );
		};

		TEST_METHOD ( ui512muldiv_01_mul )
		{
			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 num2 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 overflow [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			int adjruncount = 16;
			for ( int i = 0; i < adjruncount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
					num2 [ j ] = 0;
					product [ j ] = 0;
					overflow [ j ] = 0;
				};
				for ( int m = 7; m >= 0; m-- )
				{
					num2 [ m ] = 1;
					;
					mult_u ( product, overflow, num1, num2 );
					//{
					//	string dms = "Num1: ";
					//	for ( int kk = 0; kk < 8; kk++ )
					//	{
					//		dms += to_string ( num1 [ kk ] ) + " ";
					//	};
					//	dms += "\n";
					//	Logger::WriteMessage ( dms.c_str ( ) );
					//};
					//{
					//	string dms = "Num2: ";
					//	for ( int kk = 0; kk < 8; kk++ )
					//	{
					//		dms += to_string ( num2 [ kk ] ) + " ";
					//	};
					//	dms += "\n";
					//	Logger::WriteMessage ( dms.c_str ( ) );
					//};
					//{
					//	string dms = "Product: ";
					//	for ( int kk = 0; kk < 8; kk++ )
					//	{
					//		dms += to_string ( product [ kk ] ) + " ";
					//	};
					//	dms += "\n";
					//	Logger::WriteMessage ( dms.c_str ( ) );
					//};
					//{
					//	string dms = "Overflow: ";
					//	for ( int kk = 0; kk < 8; kk++ )
					//	{
					//		dms += to_string ( overflow [ kk ] ) + " ";
					//	};
					//	dms += "\n\n";
					//	Logger::WriteMessage ( dms.c_str ( ) );
					//};

					for ( int v = 7; v >= 0; v-- )
					{
						int pidx = v - ( 7 - m );
						int oidx = pidx + 8;
						u64 result = ( pidx >= 0 ) ? product [ pidx ] : overflow [ oidx ];
						Assert::AreEqual ( num1 [ v ], result );
					}
					num2 [ m ] = 0;
				}
			};
			string runmsg = "Multiply function testing. Ran tests " + to_string ( adjruncount * 8 ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512muldiv_01_mul_timing )
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

		TEST_METHOD ( ui512muldiv_02_mul64 )
		{
			u64 seed = 0;
			alignas ( 64 ) u64 num1 [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			alignas ( 64 ) u64 product [ 8 ] { 0, 0, 0, 0, 0, 0, 0, 0 };
			u64 num2;
			u64 overflow;
			for ( int i = 0; i < runcount; i++ )
			{
				for ( int j = 0; j < 8; j++ )
				{
					num1 [ j ] = RandomU64 ( &seed );
					product [ j ] = 0;
				};
				overflow = 0;
				num2 = RandomU64 ( &seed );
				;
				mult_uT64 ( product, &overflow, num1, &num2 );
				//{
				//	string dms = "Num1: ";
				//	for ( int kk = 0; kk < 8; kk++ )
				//	{
				//		dms += to_string ( num1 [ kk ] ) + " ";
				//	};
				//	dms += "\n";
				//	Logger::WriteMessage ( dms.c_str ( ) );
				//};
				//{
				//	string dms = "Num2: ";
				//	dms += to_string ( num2 ) + " ";
				//	dms += "\n";
				//	Logger::WriteMessage ( dms.c_str ( ) );
				//};
				//{
				//	string dms = "Product: ";
				//	for ( int kk = 0; kk < 8; kk++ )
				//	{
				//		dms += to_string ( product [ kk ] ) + " ";
				//	};
				//	dms += "\n";
				//	Logger::WriteMessage ( dms.c_str ( ) );
				//};
				//{
				//	string dms = "Overflow: ";
				//	dms += to_string ( overflow ) + " ";
				//	dms += "\n\n";
				//	Logger::WriteMessage ( dms.c_str ( ) );
				//};

				//for ( int v = 7; v >= 0; v-- )
				//{
				//	int pidx = v - ( 7 - m );
				//	int oidx = pidx + 8;
				//	u64 result = ( pidx >= 0 ) ? product [ pidx ] : overflow ;
				//	Assert::AreEqual ( num1 [ v ], result );
				//}


			};
			string runmsg = "Multiply function testing. Ran tests " + to_string ( runcount ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg.c_str ( ) );
			//	Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );

		};

		TEST_METHOD ( ui512muldiv_02_mul64_timing )
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
				mult_uT64 ( product, &overflow, num1, &num2 );
			};

			string runmsg = "Multiply function timing. Ran " + to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};
		TEST_METHOD ( ui512muldiv_03_div )
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
						//{
						//	stringstream dms;
						//	dms << "\n";
						//	dms << setw ( 12 ) << "Num1: ";
						//	for ( int kk = 0; kk < 8; kk++ )
						//	{
						//		dms << "0x" << std::hex << num1 [ kk ] << " ";
						//	};
						//	dms << "\n";
						//	Logger::WriteMessage ( dms.str ( ).c_str ( ) );
						//};
						//{
						//	stringstream dms;
						//	dms << setw ( 12 ) << "Num2: ";
						//	for ( int kk = 0; kk < 8; kk++ )
						//	{
						//		dms << "0x" << std::hex << num2 [ kk ] << " ";
						//	};
						//	dms << "\n";
						//	Logger::WriteMessage ( dms.str ( ).c_str ( ) );
						//};
						//{
						//	stringstream dms;
						//	dms << setw ( 12 ) << "Quotient: ";
						//	for ( int kk = 0; kk < 8; kk++ )
						//	{
						//		dms << "0x" << std::hex << quotient [ kk ] << " ";
						//	};
						//	dms << "\n";
						//	Logger::WriteMessage ( dms.str ( ).c_str ( ) );
						//};
						//{
						//	stringstream dms;
						//	dms << setw ( 12 ) << "Remainder: ";
						//	for ( int kk = 0; kk < 8; kk++ )
						//	{
						//		dms << "0x" << std::hex << remainder [ kk ] << " ";
						//	};
						//	dms << "\n";
						//	Logger::WriteMessage ( dms.str ( ).c_str ( ) );
						//};

						for ( int v = 7; v >= 0; v-- )
						{
							int qidx, ridx = 0;
							u64 qresult, rresult = 0;

							qidx = v - ( 7 - m );
							qresult = ( qidx >= 0 ) ? qresult = ( v >= ( 7 - m ) ) ? num1 [ qidx ] : 0ull : qresult = 0;
							rresult = ( v > m ) ? num1 [ v ] : 0ull;

							//if ( quotient [ v ] != qresult )
							//{
							//	break;
							//};
							Assert::AreEqual ( quotient [ v ], qresult, L"Quotient incorrect" );
							Assert::AreEqual ( remainder [ v ], rresult, L" Remainder incorrect" );
						};

						num2 [ m ] = 0;
					};
				};
			};
			string runmsg = "Divide function testing. Ran tests " + to_string ( adjruncount * 64 ) + " times, each with pseudo random values.\n";;
			Logger::WriteMessage ( runmsg.c_str ( ) );
			Logger::WriteMessage ( L"Passed. Tested expected values via assert.\n\n" );
		};

		TEST_METHOD ( ui512muldiv_03_div_timing )
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
				div_u ( product, overflow, num1, num2 );
			};

			string runmsg = "Divide function timing. Ran " + to_string ( timingcount ) + " times.\n";
			Logger::WriteMessage ( runmsg.c_str ( ) );
		};
		TEST_METHOD ( ui512muldiv_04_div64 )
		{

		};

		TEST_METHOD ( ui512muldiv_04_div64_timing )
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
	};
};
