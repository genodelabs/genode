/**
 * \brief  Platform_device implementation for ARM
 * \author Sebastian Sumpf
 * \date   2016-04-25
 *
 * Note: Throw away when there exists a plaform device implementation for ARM
 * in generic code
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ARM__PLATFORM_DEVICE__PLATFORM_DEVICE_H_
#define _ARM__PLATFORM_DEVICE__PLATFORM_DEVICE_H_

#include <platform_device/device.h>

#include <irq_session/connection.h>
#include <util/list.h>
#include <util/volatile_object.h>


namespace Platform { struct Device; }

struct Platform::Device : Platform::Abstract_device, Genode::List<Device>::Element
{
	unsigned                                             irq_num;
	Genode::Lazy_volatile_object<Genode::Irq_connection> irq_connection;

	Device(unsigned irq) : irq_num(irq) { }

	unsigned vendor_id() { return ~0U; }
	unsigned device_id() { return ~0U; }

	Genode::Irq_session_capability irq(Genode::uint8_t) override
	{
		irq_connection.construct(irq_num);
		return irq_connection->cap();
	}

	Genode::Io_mem_session_capability io_mem(Genode::uint8_t,
	                                         Genode::Cache_attribute,
	                                         Genode::addr_t, Genode::size_t) override
	{
		PERR("%s: not implemented", __PRETTY_FUNCTION__);
		return Genode::Io_mem_session_capability();
	}

	static Genode::List<Device> &list()
	{
		static Genode::List<Device> l;
		return l;
	}

	static Device &create(unsigned irq_num)
	{
		Device *d;
		for (d = list().first(); d; d = d->next())
			if (d->irq_num == irq_num)
				return *d;

		d = new (Genode::env()->heap()) Device(irq_num);
		list().insert(d);

		return *d;
	}
};

#endif /* _ARM__PLATFORM_DEVICE__PLATFORM_DEVICE_H_ */
