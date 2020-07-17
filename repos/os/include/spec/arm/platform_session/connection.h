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

namespace Platform { struct Connection; }


class Platform::Connection : public Genode::Connection<Session>,
                             public Client
{
	private:

		Region_map                      & _rm;
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
		: Genode::Connection<Session>(env, session(env.parent(),
		                                           "ram_quota=%u, cap_quota=%u",
		                                           RAM_QUOTA, CAP_QUOTA)),
		  Client(cap()),
		  _rm(env.rm()) { _try_attach(); }

		void update()
		{
			if (_ds.constructed() && _rom.update() == true) { return; }
			_try_attach();
		}

		void sigh(Signal_context_capability sigh) { _rom.sigh(sigh); }

		Device_capability acquire_device(String const &device) override
		{
			return retry_with_upgrade(Ram_quota{6*1024}, Cap_quota{6}, [&] () {
				return Client::acquire_device(device); });
		}

		Ram_dataspace_capability alloc_dma_buffer(size_t size) override
		{
			return retry_with_upgrade(Ram_quota{size}, Cap_quota{2}, [&] () {
				return Client::alloc_dma_buffer(size); });
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

		Device_capability device_by_index(unsigned idx)
		{
			Device_capability cap;

			with_xml([&] (Xml_node & xml) {
				try {
					Xml_node node = xml.sub_node(idx);
					Device::Name name = node.attribute_value("name",
					                                         Device::Name());
					cap = acquire_device(name.string());
				} catch(Xml_node::Nonexistent_sub_node) {
					error(__func__, ": invalid device index ", idx);
					error("device ROM content: ", xml);
				}
			});

			return cap;
		}

		Device_capability device_by_property(char const * property,
		                                     char const * value)
		{
			using String = Genode::String<64>;

			Device_capability cap;

			with_xml([&] (Xml_node & xml) {
				xml.for_each_sub_node("device", [&] (Xml_node node) {
					/* already found a device? */
					if (cap.valid()) { return; }

					bool found = false;
					node.for_each_sub_node("property", [&] (Xml_node node) {
						if ((node.attribute_value("name", String()) == property) &&
						    (node.attribute_value("value", String()) == value)) {
							found = true;
						}
					});

					if (found) {
						Device::Name name = node.attribute_value("name",
						                                         Device::Name());
						cap = acquire_device(name.string());
					}
				});
				if (!cap.valid()) {
					error(__func__, ": property=", property, " value=",
					              value, " not found!");
					error("device ROM content: ", xml);
				}
			});

			return cap;
		}
};

#endif /* _INCLUDE__SPEC__ARM__PLATFORM_SESSION__CONNECTION_H_ */
