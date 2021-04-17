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

#ifndef _INTEGER_H_
#define _INTEGER_H_

/* Genode includes */
#include <base/stdint.h>

namespace Integer {

	Genode::uint64_t u64_swap_byte_order(Genode::uint64_t input);
}

#endif /* _INTEGER_H_ */
