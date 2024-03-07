/*
 * \brief  Xoroshiro pseudo random-number generator
 * \author Josef Soentgen
 * \date   2022-05-19
 *
 * Based on Xoroshiro128+ written in 2014-2016 by Sebastiano Vigna (vigna@acm.org)
 *
 * (see http://xoroshiro.di.unimi.it/xorshift128plus.c and
 *      http://xoroshiro.di.unimi.it/splitmix64.c)
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__XOROSHIRO_H_
#define _INCLUDE__BASE__INTERNAL__XOROSHIRO_H_

#include <base/stdint.h>

namespace Genode { class Xoroshiro_128_plus; }


class Genode::Xoroshiro_128_plus
{
	private:

		uint64_t _seed;
		uint64_t _s[2] { _splitmix64(), _splitmix64() };

		uint64_t _splitmix64()
		{
			uint64_t z = (_seed += __UINT64_C(0x9E3779B97F4A7C15));
			z = (z ^ (z >> 30)) * __UINT64_C(0xBF58476D1CE4E5B9);
			z = (z ^ (z >> 27)) * __UINT64_C(0x94D049BB133111EB);
			return z ^ (z >> 31);
		}

		static uint64_t _rotl(uint64_t const x, int const k)
		{
			return (x << k) | (x >> (64 - k));
		}

	public:

		Xoroshiro_128_plus(uint64_t seed) : _seed(seed) { }

		uint64_t value()
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

#endif /* _INCLUDE__BASE__INTERNAL__XOROSHIRO_H_ */
