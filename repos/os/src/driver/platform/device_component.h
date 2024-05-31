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
#include <io_port_session/connection.h>
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
			unsigned                                idx;
			unsigned                                number;
			Irq_session::Type                       type;
			Irq_session::Polarity                   polarity;
			Irq_session::Trigger                    mode;
			bool                                    shared;
			Constructible<Irq_connection>           irq {};
			Constructible<Shared_interrupt_session> sirq {};

			Irq(Registry<Irq>       & registry,
			    unsigned              idx,
			    unsigned              number,
			    Irq_session::Type     type,
			    Irq_session::Polarity polarity,
			    Irq_session::Trigger  mode,
			    bool                  shared)
			:
				Registry<Irq>::Element(registry, *this),
				idx(idx), number(number), type(type),
				polarity(polarity), mode(mode), shared(shared) {}
		};

		struct Io_mem : Registry<Io_mem>::Element
		{
			using Pci_bar = Device::Pci_bar;

			Pci_bar                          bar;
			unsigned                         idx;
			Range                            range;
			bool                             prefetchable;
			Constructible<Io_mem_connection> io_mem {};

			Io_mem(Registry<Io_mem> & registry,
			       Pci_bar            bar,
			       unsigned           idx,
			       Range              range,
			       bool               pf)
			:
				Registry<Io_mem>::Element(registry, *this),
				bar(bar), idx(idx), range(range), prefetchable(pf) {}
		};

		struct Io_port_range : Registry<Io_port_range>::Element
		{
			using Range = Device::Io_port_range::Range;

			unsigned                          idx;
			Range                             range;
			Constructible<Io_port_connection> io_port_range {};

			Io_port_range(Registry<Io_port_range> & registry,
			              unsigned                  idx,
			              Range                     range)
			:
				Registry<Io_port_range>::Element(registry, *this),
				idx(idx), range(range) {}
		};

		struct Pci_config
		{
			addr_t addr;

			Pci_config(addr_t addr) : addr(addr) {}
		};

		Device_component(Registry<Device_component> & registry,
		                 Env                        & env,
		                 Session_component          & session,
		                 Device_model               & model,
		                 Driver::Device             & device);
		~Device_component();

		Driver::Device::Name device() const;
		Session_component  & session();
		unsigned io_mem_index(Device::Pci_bar bar);


		/************************************
		 ** Platform::Device RPC functions **
		 ************************************/

		Irq_session_capability     irq(unsigned);
		Io_mem_session_capability  io_mem(unsigned, Range &);
		Io_port_session_capability io_port_range(unsigned);

	private:

		Env                               & _env;
		Session_component                 & _session;
		Device_model                      & _device_model;
		Driver::Device::Name const          _device;
		size_t                              _cap_quota { 0 };
		size_t                              _ram_quota { 0 };
		Registry<Device_component>::Element _reg_elem;
		Registry<Irq>                       _irq_registry {};
		Registry<Io_mem>                    _io_mem_registry {};
		Registry<Io_port_range>             _io_port_range_registry {};
		Registry<Io_mem>                    _reserved_mem_registry {};
		Constructible<Pci_config>           _pci_config {};

		void _release_resources();

		/*
		 * Noncopyable
		 */
		Device_component(Device_component const &);
		Device_component &operator = (Device_component const &);
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__DEVICE_COMPONENT_H_ */
