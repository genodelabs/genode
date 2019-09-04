/*
 * \brief  Memory map specific to ARM 64
 * \author Stefan Kalkowski
 * \date   2019-09-02
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__SPEC__ARM_64__MEMORY_MAP_H_
#define _SRC__LIB__HW__SPEC__ARM_64__MEMORY_MAP_H_

#include <hw/util.h>

namespace Hw {
	namespace Mm {

		template <typename T>
		Genode::addr_t el2_addr(T t)
		{
			static constexpr Genode::addr_t OFF = 0xffffff8000000000UL;
			return (Genode::addr_t)t - OFF;
		}
	};
};

#endif /* _SRC__LIB__HW__SPEC__ARM_64__MEMORY_MAP_H_ */
