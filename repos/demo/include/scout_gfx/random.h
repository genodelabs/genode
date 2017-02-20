/*
 * \brief   Pseudo random-number generator
 * \date    2005-10-24
 * \author  Norman Feske
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SCOUT_GFX__RANDOM_H_
#define _INCLUDE__SCOUT_GFX__RANDOM_H_

namespace Scout {

	/**
	 * Produce pseudo random values
	 */
	static inline int random()
	{
		static unsigned int seed = 93186752;
		const unsigned int a = 1588635695, q = 2, r = 1117695901;
		seed = a*(seed % q) - r*(seed / q);
		return seed;
	}
}

#endif /* _INCLUDE__SCOUT_GFX__RANDOM_H_ */
