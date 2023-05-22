/*
 * \brief  Tresor-local math functions
 * \author Martin Stein
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__MATH_H_
#define _TRESOR__MATH_H_

namespace Tresor {

	template <typename T>
	constexpr T to_the_power_of(T base, T exponent)
	{
		if (exponent < 0) {
			class Negative_exponent { };
			throw Negative_exponent { };
		}
		if (exponent == 0)
			return 1;

		T result { base };
		for (T round { 1 }; round < exponent; round++)
			result *= base;

		return result;
	}
}

#endif /* _TRESOR__MATH_H_ */
