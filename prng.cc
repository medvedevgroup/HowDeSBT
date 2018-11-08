// prng.cc-- (brief) wrapper for C++ pseudo-random number generator functionality.

#include <cstdlib>
#include <cstdint>
#include <random>

#include "prng.h"

using std::mt19937;
using std::seed_seq;
#define u32 std::uint32_t
typedef unsigned int uint;

//----------
//
// seeded_prng--
//	Create a new PRNG, seeded from a string.
//
//----------
//
// Arguments:
//	char*	seed:	The seed string. The string can be any length, but
//					characters beyond a certain length are ignored. If the
//					string is empty, another allegedly random source is used
//					as the seed.
//
// Returns:
//	The random number generator object.
//
//----------
//
// Impementation Notes:
//	[1]	The state vector is initialized using the linear congruence generator
//			x <- ((69069x + 2*seed[i]) | 1) mod 2^32
//		When the end of the seed string is reached, the remaining seed[i]
//		values are considered to be zero. At that point, the remainder of the
//		state vector is similar to the generator from Line 15 of Table 1, page
//		106, section 3.3.4 of Knuth's "The Art of Computer Programming", volume
//		2, 3rd edition. The use of that sequence is based on the (apparent)
//		recommendation of Shawn Cokus appearing in many pre-C++ implementations
//		of Mersenne Twister.
//
//----------

mt19937* seeded_prng
   (const std::string& seed)
	{
	mt19937* prng;

	if (seed == "")
		{
		std::random_device randomDevice;
		prng = new mt19937(randomDevice());
		}
	else
		{
		u32 seedData[mt19937::state_size];
		u32 x = 13013;

		for (uint ix=0 ; ix<mt19937::state_size ; ix++)
			{
			if (ix < seed.length()) x = (x * 69069) + (2*seed[ix]);
			                   else x = (x * 69069);
			x |= 1;
			seedData[ix] = x;
			}

		seed_seq seedSequence(std::begin(seedData), std::end(seedData));
		prng = new mt19937(seedSequence);
		}

	return prng;
	}
