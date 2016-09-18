/*
 * \brief  Simple random number generator for lwIP
 * \author Emery Hemingway
 * \date   2016-07-30
 */

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

/* Genode includes */
#include <trace/timestamp.h>
#include <base/fixed_stdint.h>

extern "C"
genode_uint32_t genode_rand()
{
	using namespace Genode;

	static uint64_t const inc = Trace::timestamp()|1;
	static uint64_t     state = Trace::timestamp();
	uint64_t oldstate = state;

	// Advance internal state
	state = oldstate * 6364136223846793005ULL + inc;

	// Calculate output function (XSH RR), uses old state for max ILP
	uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
	uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}
