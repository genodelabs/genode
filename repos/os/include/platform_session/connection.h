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

#ifndef _INCLUDE__PLATFORM_SESSION__CONNECTION_H_
#define _INCLUDE__PLATFORM_SESSION__CONNECTION_H_

#include <base/attached_dataspace.h>
#include <base/connection.h>
#include <base/env.h>
#include <base/signal.h>
#include <platform_session/client.h>
#include <rom_session/client.h>
#include <util/xml_node.h>

namespace Platform {

	struct Device;
	struct Dma_buffer;
	struct Connection;
}


class Platform::Connection : public Genode::Connection<Session>,
                             public Client
{
	private:

		/* 'Device' and 'Dma_buffer' access the '_env' member */
		friend class Device;
		friend class Dma_buffer;

		Env                             & _env;
		Rom_session_client                _rom     { devices_rom() };
		Constructible<Attached_dataspace> _ds      {};
		Io_signal_handler<Connection>     _handler { _env.ep(), *this,
		                                             &Connection::_handle_io };

		void _try_attach()
		{
			_ds.destruct();
			try { _ds.construct(_env.rm(), _rom.dataspace()); }
			catch (Attached_dataspace::Invalid_dataspace) {
				warning("Invalid devices rom dataspace returned!");}
		}

		void _handle_io() {}

		template <typename FN>
		Capability<Device_interface> _wait_for_device(FN const & fn)
		{
			for (;;) {
				/* repeatedly check for availability of device */
				Capability<Device_interface> cap = fn();
				if (cap.valid())
					return cap;

				_env.ep().wait_and_dispatch_one_io_signal();
			}
		}

	public:

		Connection(Env &env)
		:
			Genode::Connection<Session>(env, Label(), Ram_quota { 32*1024 }, Args()),
			Client(cap()),
			_env(env)
		{
			_try_attach();

			/*
			 * Initially register dummy handler, to be able to receive signals
			 * if _wait_for_device probes for a valid devices rom
			 */
			sigh(_handler);
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
			return _wait_for_device([&] () {
				return retry_with_upgrade(Ram_quota{6*1024}, Cap_quota{6}, [&] () {
					return Client::acquire_device(name); });
			});
		}

		Capability<Device_interface> acquire_device()
		{
			return _wait_for_device([&] () {
				return retry_with_upgrade(Ram_quota{6*1024}, Cap_quota{6}, [&] () {
					return Client::acquire_single_device(); });
			});
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
			return _wait_for_device([&] () {

				using String = Genode::String<64>;

				Capability<Device_interface> cap;

				update();
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
				});

				return cap;
			});
		}
};

#endif /* _INCLUDE__PLATFORM_SESSION__CONNECTION_H_ */
