/*
 * \brief  Client connection to USB server
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__CONNECTION_H_
#define _INCLUDE__USB_SESSION__CONNECTION_H_

#include <base/attached_dataspace.h>
#include <base/connection.h>
#include <rom_session/client.h>
#include <usb_session/client.h>

namespace Usb { struct Connection; }


class Usb::Connection : public Genode::Connection<Session>, public Usb::Client
{
	private:

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

		void _handle_io() { }

		template <typename FN>
		Device_capability _wait_for_device(FN const & fn)
		{
			for (;;) {
				/* repeatedly check for availability of device */
				Device_capability cap = fn();
				if (cap.valid())
					return cap;

				_env.ep().wait_and_dispatch_one_io_signal();
			}
		}

	public:

		Connection(Genode::Env    &env,
		           Genode::size_t  ram_quota = RAM_QUOTA)
		:
			Genode::Connection<Session>(env, Label(),
			                            Ram_quota { ram_quota }, Args()),
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

		template <typename FN>
		void with_xml(FN const & fn)
		{
			update();
			try {
				if (_ds.constructed() && _ds->local_addr<void const>()) {
					Xml_node xml(_ds->local_addr<char>(), _ds->size());
					fn(xml);
				}
			}  catch (Xml_node::Invalid_syntax) {
				warning("Devices rom has invalid XML syntax"); }
		}

		Device_capability acquire_device(Device_name const &name) override
		{
			Ram_quota ram_quota(Device_session::TX_BUFFER_SIZE + 4096);
			return retry_with_upgrade(ram_quota, Cap_quota{6}, [&] () {
				return Client::acquire_device(name); });
		}

		Device_capability acquire_device()
		{
			return _wait_for_device([&] () {
				Ram_quota ram_quota(Device_session::TX_BUFFER_SIZE + 4096);
				return retry_with_upgrade(ram_quota, Cap_quota{6}, [&] () {
					return Client::acquire_single_device(); });
			});
		}
};

#endif /* _INCLUDE__USB_SESSION__CONNECTION_H_ */
