/*
 * \brief  Global interface for handling reserved memory regions
 * \author Johannes Schlatow
 * \date   2024-06-06
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__RESERVED_MEMORY_HANDLER_H_
#define _SRC__DRIVERS__PLATFORM__RESERVED_MEMORY_HANDLER_H_

#include <platform_session/device.h>

namespace Driver
{
	using Range = Platform::Device_interface::Range;

	struct Device;
	struct Reserved_memory_handler;
}


struct Driver::Reserved_memory_handler
{
	virtual void add_range(Device const &, Range const &)    { }
	virtual void remove_range(Device const &, Range const &) { }
	virtual ~Reserved_memory_handler() { }
};

#endif /* _SRC__DRIVERS__PLATFORM__RESERVED_MEMORY_HANDLER_H_ */
