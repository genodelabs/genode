/*
 * \brief  Interface between kernel and userland
 * \author Martin Stein
 * \date   2011-11-30
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
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
			SP            = 13,  /* XXX numbers are arbitrary, taken from ARM version */
			IP            = 15,
			FAULT_TLB     = 18,
			FAULT_ADDR    = 19,
			FAULT_WRITES  = 20,
			FAULT_SIGNAL  = 21,
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

