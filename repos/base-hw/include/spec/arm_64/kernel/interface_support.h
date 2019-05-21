/*
 * \brief  Interface between kernel and userland
 * \author Stefan Kalkowski
 * \date   2019-05-09
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM_64__KERNEL__INTERFACE_SUPPORT_H_
#define _INCLUDE__SPEC__ARM_64__KERNEL__INTERFACE_SUPPORT_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel
{
	typedef Genode::uint64_t Call_arg;
	typedef Genode::uint64_t Call_ret;
	typedef Genode::uint64_t Call_ret_64;
}

#endif /* _INCLUDE__SPEC__ARM_64__KERNEL__INTERFACE_SUPPORT_H_ */
