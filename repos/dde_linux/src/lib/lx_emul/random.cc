/*
 * \brief  Source of randomness in lx_emul
 * \author Josef Soentgen
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \author Christian Helmuth
 * \date   2022-05-19
 *
 * :Warning:
 *
 * The output of the Xoroshiro128+ PRNG that is used in the implementation of
 * the lx_emul randomness functions has known statistical problems (see
 * https://en.wikipedia.org/wiki/Xoroshiro128%2B#Statistical_Quality).
 * Furthermore, the integration of Xoroshir128+ with the lx_emul code was not
 * reviewed/audited for its security-related properties, so far. Thus,
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


class Entropy_source : Interface
{
	public:

		virtual uint64_t gen_random_u64() = 0;
};


/*
 * A wrapper for the Xoroshiro128+ PRNG that reseeds the PRNG every
 * 1024 * 1024 + random(0..4095) bytes of generated output.
 */
class Xoroshiro_128_plus_reseeding
{
	private:

		enum { NR_OF_GEN_BYTES_BASE_LIMIT = 1024 * 1024 };

		Entropy_source                    &_entropy_src;
		uint64_t                           _seed                  { 0 };
		size_t                             _nr_of_gen_bytes       { 0 };
		size_t                             _nr_of_gen_bytes_limit { 0 };
		Constructible<Xoroshiro_128_plus>  _xoroshiro             { };

		void _reseed()
		{
			_seed = _entropy_src.gen_random_u64();
			_nr_of_gen_bytes = 0;
			_nr_of_gen_bytes_limit =
				NR_OF_GEN_BYTES_BASE_LIMIT + (_seed & 0xfff);

			_xoroshiro.construct(_seed);
		}

	public:

		Xoroshiro_128_plus_reseeding(Entropy_source &entropy_src)
		:
			_entropy_src { entropy_src }
		{
			_reseed();
		}

		uint64_t get_u64()
		{
			_nr_of_gen_bytes += 8;
			if (_nr_of_gen_bytes >= _nr_of_gen_bytes_limit) {
				_reseed();
				_nr_of_gen_bytes += 8;
			}
			return _xoroshiro->get_u64();
		}
};


class Jitterentropy : public Entropy_source
{
	private:

		bool _initialized { false };

		rand_data *_rand_data { nullptr };

		Jitterentropy(Jitterentropy const &) = delete;

		Jitterentropy & operator = (Jitterentropy const &) = delete;

	public:

		Jitterentropy(Allocator &alloc)
		{
			jitterentropy_init(alloc);

			int err = jent_entropy_init();
			if (err != 0) {
				warning("jitterentropy: initialization error (", err,
				        ") randomness is poor quality");
				return;
			}

			_rand_data = jent_entropy_collector_alloc(0, 0);
			if (_rand_data == nullptr) {
				error("jitterentropy could not allocate entropy collector!");
			}
			_initialized = true;
		}


		/********************
		 ** Entropy_source **
		 ********************/

		uint64_t gen_random_u64() override
		{
			uint64_t result;

			/* jitterentropy initialization failed and cannot be used, return something */
			if (!_initialized) {
				static uint64_t counter = 1;
				return (uint64_t)&result * ++counter;
			}

			jent_read_entropy(_rand_data, (char*)&result, sizeof(result));
			return result;
		}
};


static Xoroshiro_128_plus_reseeding &xoroshiro()
{
	static Jitterentropy                jitterentropy { Lx_kit::env().heap };
	static Xoroshiro_128_plus_reseeding xoroshiro     { jitterentropy };
	return xoroshiro;
}


void lx_emul_random_gen_bytes(void          *dst,
                              unsigned long  nr_of_bytes)
{
	/* validate arguments */
	if (dst == nullptr || nr_of_bytes == 0) {
		error(__func__, " called with invalid args!");
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


genode_uint32_t lx_emul_random_gen_u32()
{
	return (genode_uint32_t)xoroshiro().get_u64();
}


genode_uint64_t lx_emul_random_gen_u64()
{
	return (genode_uint64_t)xoroshiro().get_u64();
}
