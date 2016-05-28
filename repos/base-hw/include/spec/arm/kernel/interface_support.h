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

#ifndef _INCLUDE__SPEC__ARM__KERNEL__INTERFACE_SUPPORT_H_
#define _INCLUDE__SPEC__ARM__KERNEL__INTERFACE_SUPPORT_H_

/* Genode includes */
#include <base/stdint.h>

namespace Kernel
{
	typedef Genode::uint32_t Call_arg;
	typedef Genode::uint32_t Call_ret;

	/**
	 * Events that are provided by a kernel thread-object for user handling
	 */
	struct Thread_event_id
	{
		enum { FAULT = 0 };
	};
}

#endif /* _INCLUDE__SPEC__ARM__KERNEL__INTERFACE_SUPPORT_H_ */
