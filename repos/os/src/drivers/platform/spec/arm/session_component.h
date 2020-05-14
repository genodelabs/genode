/*
 * \brief  Platform driver - session component
 * \author Stefan Kalkowski
 * \date   2020-04-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__SPEC__ARM__SESSION_COMPONENT_H_
#define _SRC__DRIVERS__PLATFORM__SPEC__ARM__SESSION_COMPONENT_H_

#include <base/env.h>
#include <base/heap.h>
#include <base/quota_guard.h>
#include <base/registry.h>
#include <base/session_object.h>
#include <os/dynamic_rom_session.h>
#include <os/session_policy.h>
#include <platform_session/platform_session.h>

#include <device_component.h>

namespace Driver {
	class Session_component;
	class Root;
}


class Driver::Session_component :
	public  Genode::Session_object<Platform::Session>,
	private Genode::Registry<Driver::Session_component>::Element,
	private Genode::Dynamic_rom_session::Xml_producer
{
	public:

		using Registry = Genode::Registry<Session_component>;

		Session_component(Genode::Env     & env,
		                  Device_model    & devices,
		                  Registry        & registry,
		                  Label     const & label,
		                  Resources const & resources,
		                  Diag      const & diag);
		~Session_component();

		Genode::Heap & heap();
		Genode::Env  & env();
		Device_model & devices();

		void     add(Device::Name const &);
		bool     has_device(Device::Name const &) const;
		unsigned devices_count() const;

		Genode::Ram_quota_guard &ram_quota_guard() {
			return _ram_quota_guard(); }

		Genode::Cap_quota_guard &cap_quota_guard() {
			return _cap_quota_guard(); }


		/**************************
		 ** Platform Session API **
		 **************************/

		using Rom_session_capability   = Genode::Rom_session_capability;
		using Device_capability        = Platform::Device_capability;
		using Ram_dataspace_capability = Genode::Ram_dataspace_capability;
		using String                   = Platform::Session::String;
		using size_t                   = Genode::size_t;

		Rom_session_capability devices_rom() override;
		Device_capability acquire_device(String const &) override;
		void release_device(Device_capability) override;
		Ram_dataspace_capability alloc_dma_buffer(size_t const) override;
		void free_dma_buffer(Ram_dataspace_capability ram_cap) override;
		Genode::addr_t bus_addr_dma_buffer(Ram_dataspace_capability) override;

	private:

		friend class Root;

		struct Dma_buffer : Genode::List<Dma_buffer>::Element
		{
			Genode::Ram_dataspace_capability const cap;

			Dma_buffer(Genode::Ram_dataspace_capability const cap)
			: cap(cap) {}
		};

		using Device_list_element = Genode::List_element<Device_component>;
		using Device_list         = Genode::List<Device_list_element>;

		Genode::Env                     & _env;
		Genode::Constrained_ram_allocator _env_ram     { _env.pd(),
		                                                 _ram_quota_guard(),
		                                                 _cap_quota_guard()  };
		Genode::Heap                      _md_alloc    { _env_ram, _env.rm() };
		Device_list                       _device_list { };
		Genode::List<Dma_buffer>          _buffer_list { };
		Genode::Dynamic_rom_session       _rom_session { _env.ep(), _env.ram(),
		                                                 _env.rm(), *this    };
		Device_model                    & _device_model;

		/*
		 * Noncopyable
		 */
		Session_component(Session_component const &);
		Session_component &operator = (Session_component const &);


		/*******************************************
		 ** Dynamic_rom_session::Xml_producer API **
		 *******************************************/

		void produce_xml(Genode::Xml_generator &xml) override;
};

#endif /* _SRC__DRIVERS__PLATFORM__SPEC__ARM__SESSION_COMPONENT_H_ */
