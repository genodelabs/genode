/*
 * \brief  UART LOG component
 * \author Christian Helmuth
 * \date   2011-05-30
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _UART_COMPONENT_H_
#define _UART_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <util/arg_string.h>
#include <os/session_policy.h>
#include <os/attached_ram_dataspace.h>
#include <root/component.h>
#include <uart_session/uart_session.h>

/* local includes */
#include "uart_driver.h"

namespace Uart {

	using namespace Genode;

	class Session_component : public Rpc_object<Uart::Session,
	                                            Session_component>
	{
		private:

			/*
			 * XXX Do not use hard-coded value, better make it dependent
			 *     on the RAM quota donated by the client.
			 */
			enum { IO_BUFFER_SIZE = 4096 };

			Genode::Attached_ram_dataspace _io_buffer;

			/**
			 * Functor informing the client about new data to read
			 */
			struct Char_avail_callback : Uart::Char_avail_callback
			{
				Genode::Signal_context_capability sigh;

				void operator ()()
				{
					if (sigh.valid())
						Genode::Signal_transmitter(sigh).submit();
				}

			} _char_avail_callback;

			Uart::Driver_factory &_driver_factory;
			Uart::Driver         &_driver;

		public:

			/**
			 * Constructor
			 */
			Session_component(Uart::Driver_factory &driver_factory,
			                  unsigned index, unsigned baudrate)
			:
				_io_buffer(Genode::env()->ram_session(), IO_BUFFER_SIZE),
				_driver_factory(driver_factory),
				_driver(*_driver_factory.create(index, baudrate, _char_avail_callback))
			{ }


			/****************************
			 ** Uart session interface **
			 ****************************/

			void baud_rate(Genode::size_t bits_per_second)
			{
				_driver.baud_rate(bits_per_second);
			}


			/********************************
			 ** Terminal session interface **
			 ********************************/

			Size size() { return Size(0, 0); }

			bool avail() { return _driver.char_avail(); }

			Genode::size_t _read(Genode::size_t dst_len)
			{
				char *io_buf      = _io_buffer.local_addr<char>();
				Genode::size_t sz = Genode::min(dst_len, _io_buffer.size());

				Genode::size_t n = 0;
				while ((n < sz) && _driver.char_avail())
					io_buf[n++] = _driver.get_char();

				return n;
			}

			void _write(Genode::size_t num_bytes)
			{
				/* constain argument to I/O buffer size */
				num_bytes = Genode::min(num_bytes, _io_buffer.size());

				char const *io_buf = _io_buffer.local_addr<char>();
				for (Genode::size_t i = 0; i < num_bytes; i++)
					_driver.put_char(io_buf[i]);
			}

			Genode::Dataspace_capability _dataspace()
			{
				return _io_buffer.cap();
			}

			void connected_sigh(Genode::Signal_context_capability sigh)
			{
				/*
				 * Immediately reflect connection-established signal to the
				 * client because the session is ready to use immediately after
				 * creation.
				 */
				Genode::Signal_transmitter(sigh).submit();
			}

			void read_avail_sigh(Genode::Signal_context_capability sigh)
			{
				_char_avail_callback.sigh = sigh;

				if (_driver.char_avail())
					_char_avail_callback();
			}

			Genode::size_t read(void *, Genode::size_t) { return 0; }
			Genode::size_t write(void const *, Genode::size_t) { return 0; }
	};


	typedef Root_component<Session_component, Multiple_clients> Root_component;


	class Root : public Root_component
	{
		private:

			Driver_factory &_driver_factory;

		protected:

			Session_component *_create_session(const char *args)
			{
				try {
					Session_policy policy(args);

					unsigned uart_index = 0;
					policy.attribute("uart").value(&uart_index);

					unsigned uart_baudrate = 0;
					try {
						policy.attribute("baudrate").value(&uart_baudrate);
					} catch (Xml_node::Nonexistent_attribute) {
						PDBG("Missing \"baudrate\" attribute in policy definition");
					}

					PDBG("UART%d %d", uart_index, uart_baudrate);

					return new (md_alloc())
						Session_component(_driver_factory, uart_index, uart_baudrate);

				} catch (Xml_node::Nonexistent_attribute) {
					PERR("Missing \"uart\" attribute in policy definition");
					throw Root::Unavailable();
				} catch (Session_policy::No_policy_defined) {
					PERR("Invalid session request, no matching policy");
					throw Root::Unavailable();
				}
			}

		public:

			/**
			 * Constructor
			 */
			Root(Rpc_entrypoint *ep, Allocator *md_alloc, Driver_factory &driver_factory)
			:
				Root_component(ep, md_alloc), _driver_factory(driver_factory)
			{ }
	};
}

#endif /* _UART_COMPONENT_H_ */
