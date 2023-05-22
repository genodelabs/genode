/*
 * \brief  Helper functions for modifying integer values
 * \author Martin Stein
 * \date   2021-04-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <integer.h>

using namespace Genode;


uint64_t Integer::u64_swap_byte_order(uint64_t input)
{
	uint64_t result { 0 };
	for (unsigned byte_idx = 0; byte_idx < 8; byte_idx++) {
		uint8_t const byte { (uint8_t)(input >> (byte_idx * 8)) };
		result |= (uint64_t)byte << ((7 - byte_idx) * 8);
	}
	return result;
}
