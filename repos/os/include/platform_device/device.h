/*
 * \brief  Abstract platform device interface
 * \author Alexander Boettcher
 * \date   2015-03-15
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM_DEVICE__DEVICE_H_
#define _INCLUDE__PLATFORM_DEVICE__DEVICE_H_

#include <util/interface.h>
#include <base/cache.h>
#include <irq_session/capability.h>
#include <io_mem_session/capability.h>

namespace Platform { class Abstract_device; }

struct Platform::Abstract_device : Genode::Interface
{
	/**
	 * Get IRQ session capability
	 */
	virtual	Genode::Irq_session_capability irq(Genode::uint8_t) = 0;

	/**
	 * Get IO mem session capability of specified resource id
	 */
	virtual Genode::Io_mem_session_capability io_mem(Genode::uint8_t,
	                                                 Genode::Cache_attribute,
	                                                 Genode::addr_t, Genode::size_t) = 0;
};

#endif /* _INCLUDE__PLATFORM_DEVICE__DEVICE_H_ */
