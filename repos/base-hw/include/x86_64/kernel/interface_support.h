/*
 * \brief  Interface between kernel and userland
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__INTERFACE_SUPPORT_H_
#define _KERNEL__INTERFACE_SUPPORT_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel
{
	typedef Genode::uint64_t Call_arg;
	typedef Genode::uint64_t Call_ret;

	/**
	 * Registers that are provided by a kernel thread-object for user access
	 */
	struct Thread_reg_id
	{
		enum {
			IP           = 0,
			SP           = 1,
			FAULT_TLB    = 2,
			FAULT_ADDR   = 3,
			FAULT_WRITES = 4,
			FAULT_SIGNAL = 5,
		};
	};

	/**
	 * Events that are provided by a kernel thread-object for user handling
	 */
	struct Thread_event_id
	{
		enum { FAULT = 0 };
	};
}

#endif /* _KERNEL__INTERFACE_SUPPORT_H_ */

