/*
 * \brief  Platform driver for ARM device component
 * \author Stefan Kalkowski
 * \date   2020-04-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_
#define _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_

#include <base/registry.h>
#include <base/rpc_server.h>
#include <io_mem_session/connection.h>
#include <irq_session/connection.h>
#include <platform_session/device.h>
#include <platform_session/platform_session.h>
#include <util/reconstructible.h>

#include <device.h>

namespace Driver {
	class Device_component;
	class Session_component;
}


class Driver::Device_component : public Rpc_object<Platform::Device_interface,
                                                   Device_component>
{
	public:

		struct Irq : Registry<Irq>::Element
		{
			unsigned                      idx;
			unsigned                      number;
			Constructible<Irq_connection> irq {};

			Irq(Registry<Irq> & registry,
			    unsigned        idx,
			    unsigned        number)
			:
				Registry<Irq>::Element(registry, *this),
				idx(idx), number(number) {}
		};

		struct Io_mem : Registry<Io_mem>::Element
		{
			unsigned                         idx;
			Range                            range;
			Constructible<Io_mem_connection> io_mem {};

			Io_mem(Registry<Io_mem> & registry,
			       unsigned           idx,
			       Range              range)
			:
				Registry<Io_mem>::Element(registry, *this),
				idx(idx), range(range) {}
		};

		Device_component(Registry<Device_component> & registry,
		                 Session_component          & session,
		                 Driver::Device             & device);
		~Device_component();

		Driver::Device::Name device() const;
		Session_component  & session();


		/************************************
		 ** Platform::Device RPC functions **
		 ************************************/

		Irq_session_capability    irq(unsigned);
		Io_mem_session_capability io_mem(unsigned, Range &, Cache);

	private:

		Session_component                 & _session;
		Driver::Device::Name const          _device;
		size_t                              _cap_quota { 0 };
		size_t                              _ram_quota { 0 };
		Registry<Device_component>::Element _reg_elem;
		Registry<Irq>                       _irq_registry {};
		Registry<Io_mem>                    _io_mem_registry {};

		void _release_resources();

		/*
		 * Noncopyable
		 */
		Device_component(Device_component const &);
		Device_component &operator = (Device_component const &);
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_ */
