/*
 * \brief  Connection to Platform service
 * \author Stefan Kalkowski
 * \date   2020-05-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CONNECTION_H_
#define _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CONNECTION_H_

#include <base/attached_dataspace.h>
#include <base/connection.h>
#include <base/env.h>
#include <platform_session/client.h>
#include <rom_session/client.h>
#include <util/xml_node.h>

namespace Platform {

	struct Device;
	struct Connection;
}


class Platform::Connection : public Genode::Connection<Session>,
                             public Client
{
	private:

		friend class Device; /* 'Device' accesses '_rm' */

		Region_map                       &_rm;
		Rom_session_client                _rom {devices_rom()};
		Constructible<Attached_dataspace> _ds  {};

		void _try_attach()
		{
			_ds.destruct();
			try { _ds.construct(_rm, _rom.dataspace()); }
			catch (Attached_dataspace::Invalid_dataspace) {
				warning("Invalid devices rom dataspace returned!");}
		}

	public:

		Connection(Env &env)
		:
			Genode::Connection<Session>(env, session(env.parent(),
			                                           "ram_quota=%u, cap_quota=%u",
			                                           RAM_QUOTA, CAP_QUOTA)),
			Client(cap()),
			_rm(env.rm())
		{
			_try_attach();
		}

		void update()
		{
			if (_ds.constructed() && _rom.update() == true)
				return;

			_try_attach();
		}

		void sigh(Signal_context_capability sigh) { _rom.sigh(sigh); }

		Capability<Device_interface> acquire_device(Device_name const &name) override
		{
			return retry_with_upgrade(Ram_quota{6*1024}, Cap_quota{6}, [&] () {
				return Client::acquire_device(name); });
		}

		Capability<Device_interface> acquire_device()
		{
			return retry_with_upgrade(Ram_quota{6*1024}, Cap_quota{6}, [&] () {
				return Client::acquire_single_device(); });
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size, Cache cache) override
		{
			return retry_with_upgrade(Ram_quota{size}, Cap_quota{2}, [&] () {
				return Client::alloc_dma_buffer(size, cache); });
		}

		template <typename FN>
		void with_xml(FN const & fn)
		{
			try {
				if (_ds.constructed() && _ds->local_addr<void const>()) {
					Xml_node xml(_ds->local_addr<char>(), _ds->size());
					fn(xml);
				}
			}  catch (Xml_node::Invalid_syntax) {
				warning("Devices rom has invalid XML syntax"); }
		}

		Capability<Device_interface> device_by_type(char const * type)
		{
			using String = Genode::String<64>;

			Capability<Device_interface> cap;

			with_xml([&] (Xml_node & xml) {
				xml.for_each_sub_node("device", [&] (Xml_node node) {

					/* already found a device? */
					if (cap.valid())
						return;

					if (node.attribute_value("type", String()) != type)
						return;

					Device_name name = node.attribute_value("name", Device_name());
					cap = acquire_device(name);
				});
				if (!cap.valid()) {
					error(__func__, ": type=", type, " not found!");
					error("device ROM content: ", xml);
				}
			});

			return cap;
		}
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CONNECTION_H_ */
