/*
 * \brief  Dimensioning of the hierarchic 'Chunk' structure for the RAM fs
 * \author Norman Feske
 * \date   2019-03-17
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RAM_FS__PARAM_H_
#define _INCLUDE__RAM_FS__PARAM_H_

/* Genode includes */
#include <base/stdint.h>

namespace Ram_fs
{
	using Genode::size_t;

	/*
	 * Let number of level-0 and level-1 entries depend on the machine-word
	 * size to allow the use of more than 2 GiB on 64-bit machines.
	 *
	 * 32 bit: 64*64*128*4096   = 2 GiB
	 * 64 bit: 128*128*128*4096 = 8 GiB
	 */
	static constexpr size_t num_level_0_entries()
	{
		return sizeof(size_t) == 8 ? 128 : 64;
	}

	static constexpr size_t num_level_1_entries() { return num_level_0_entries(); }
	static constexpr size_t num_level_2_entries() { return 128; }
	static constexpr size_t num_level_3_entries() { return 4096; }
}

#endif /* _INCLUDE__RAM_FS__PARAM_H_ */

