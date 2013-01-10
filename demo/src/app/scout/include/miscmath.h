/*
 * \brief   Misc math functions used here and there
 * \date    2005-10-24
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MISCMATH_H_
#define _MISCMATH_H_

/**
 * Calc min/max of two numbers
 */
template <typename T> static inline T min(T a, T b) { return a < b ? a : b; }
template <typename T> static inline T max(T a, T b) { return a > b ? a : b; }


/**
 * Produce pseudo random values
 */
static inline int random(void)
{
	static unsigned int seed = 93186752;
	const unsigned int a = 1588635695, q = 2, r = 1117695901;
	seed = a*(seed % q) - r*(seed / q);
	return seed;
}

#endif /* _MISCMATH_H_ */
