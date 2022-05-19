/*
 * \brief  Source of randomness in lx_emul
 * \author Josef Soentgen
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2022-05-19
 *
 * :Warning:
 *
 * The output of the Xoroshiro128+ PRNG that is used in the implementation of
 * the lx_emul randomness functions has known statistical problems (see
 * https://en.wikipedia.org/wiki/Xoroshiro128%2B#Statistical_Quality).
 * Furthermore, the integration of Xoroshir128+ with the lx_emul code was not
 * reviewed/audited for its security-related properties, so far, and has the
 * known deficiency of seeding the PRNG only once during initialization. Thus,
 * we strongly advise against the use of the lx_emul randomness functions for
 * security-critical purposes.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* base/include */
#include <base/log.h>
#include <base/fixed_stdint.h>
#include <util/string.h>

/* dde_linux/src/include */
#include <lx_emul/random.h>
#include <lx_kit/env.h>

/* contrib/linux/src/linux/crypto */
#include <jitterentropy.h>

using namespace Genode;

/*
 * Xoroshiro128+ written in 2014-2016 by Sebastiano Vigna (vigna@acm.org)
 *
 * (see http://xoroshiro.di.unimi.it/xorshift128plus.c and
 *      http://xoroshiro.di.unimi.it/splitmix64.c)
 */
class Xoroshiro_128_plus
{
	private:

		uint64_t _seed;
		uint64_t _s[2];

		uint64_t _splitmix64()
		{
			uint64_t z = (_seed += __UINT64_C(0x9E3779B97F4A7C15));
			z = (z ^ (z >> 30)) * __UINT64_C(0xBF58476D1CE4E5B9);
			z = (z ^ (z >> 27)) * __UINT64_C(0x94D049BB133111EB);
			return z ^ (z >> 31);
		}

		static uint64_t _rotl(uint64_t const x, int k)
		{
			return (x << k) | (x >> (64 - k));
		}

	public:

		Xoroshiro_128_plus(uint64_t seed) : _seed(seed)
		{
			_s[0] = _splitmix64();
			_s[1] = _splitmix64();
		}

		uint64_t get_u64()
		{
			uint64_t const s0 = _s[0];
			uint64_t       s1 = _s[1];
			uint64_t const result = s0 + s1;

			s1 ^= s0;

			_s[0] = _rotl(s0, 55) ^ s1 ^ (s1 << 14);
			_s[1] = _rotl(s1, 36);

			return result;
		}
};

static uint64_t jitterentropy_gen_random_u64()
{
	static rand_data *jent { nullptr };
	if (jent == nullptr) {
		jitterentropy_init(Lx_kit::env().heap);

		if (jent_entropy_init() != 0) {
			Genode::error("jitterentropy library could not be initialized!");
		}
		jent = jent_entropy_collector_alloc(0, 0);
		if (jent == nullptr) {
			Genode::error("jitterentropy could not allocate entropy collector!");
		}
	}
	uint64_t result;
	jent_read_entropy(jent, (char*)&result, sizeof(result));
	return result;
}


static Xoroshiro_128_plus &xoroshiro()
{
	static Xoroshiro_128_plus xoroshiro { jitterentropy_gen_random_u64() };
	return xoroshiro;
}


void lx_emul_gen_random_bytes(void          *dst,
                              unsigned long  nr_of_bytes)
{
	/* validate arguments */
	if (dst == nullptr || nr_of_bytes == 0) {
		Genode::error("lx_emul_gen_random_bytes called with invalid args!");
		return;
	}
	/* fill up the destination with random 64-bit values as far as possible */
	char *dst_char { reinterpret_cast<char*>(dst) };
	unsigned long const nr_of_rounds { nr_of_bytes >> 3 };
	for (unsigned long round_idx { 0 }; round_idx < nr_of_rounds; round_idx++) {

		uint64_t const rand_u64 { xoroshiro().get_u64() };
		Genode::memcpy(dst_char, &rand_u64, 8);
		dst_char += 8;
	}
	/* fill up remaining bytes from one additional random 64-bit value */
	nr_of_bytes -= (nr_of_rounds << 3);
	if (nr_of_bytes == 0) {
		return;
	}
	uint64_t const rand_u64 { xoroshiro().get_u64() };
	Genode::memcpy(dst_char, &rand_u64, nr_of_bytes);

}


unsigned int lx_emul_gen_random_uint()
{
	return (unsigned int)xoroshiro().get_u64();
}
