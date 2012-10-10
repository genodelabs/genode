/*
 * \brief   Misc math functions used here and there
 * \date    2010-09-27
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NANO3D__MISC_MATH_H_
#define _INCLUDE__NANO3D__MISC_MATH_H_

namespace Nano3d {

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


	/**
	 * Tables containing 16.16 fixpoint values 
	 */
	enum { SINCOSTAB_SIZE = 1024 };
	extern int sintab[];
	extern int costab[];

	inline int sin(int angle) { return sintab[angle & (SINCOSTAB_SIZE - 1)]; }
	inline int cos(int angle) { return costab[angle & (SINCOSTAB_SIZE - 1)]; }

	/**
	 * Init sine/cosine table
	 */
	extern void init_sincos_tab();


	/**
	 * Calculate square root of an integer value
	 */
	template <typename T>
	T sqrt(T value)
	{
		/*
		 * Calculate square root using nested intervalls. The range of values
		 * is log(x) with x being the maximum value of type T. We narrow the
		 * result bit by bit starting with the most significant bit.
		 */
		T result = 0;
		for (T i = sizeof(T)*8 / 2; i > 0; i--) {
			T const bit  = i - 1;
			T const test = result + (1 << bit);
			if (test*test <= value)
				result = test;
		}
		return result;
	}
}

#endif /* _INCLUDE__NANO3D__MISC_MATH_H_ */
