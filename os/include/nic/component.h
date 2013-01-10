/*
 * \brief  Glue between device-specific NIC driver code and Genode
 * \author Norman Feske
 * \date   2011-05-21
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC__COMPONENT_H_
#define _INCLUDE__NIC__COMPONENT_H_

#include <base/env.h>
#include <nic_session/rpc_object.h>
#include <base/allocator_avl.h>
#include <util/arg_string.h>
#include <base/rpc_server.h>
#include <root/component.h>
#include <nic/driver.h>

enum { VERBOSE_RX = false };

namespace Nic {

	class Session_component : public Genode::Allocator_avl,
	                          public Session_rpc_object, public Rx_buffer_alloc
	{
		private:

			Driver_factory &_driver_factory;
			Driver         &_driver;

			/* rx packet descriptor */
			Packet_descriptor _curr_rx_packet;

			enum { TX_STACK_SIZE = 8*1024 };
			class Tx_thread : public Genode::Thread<TX_STACK_SIZE>
			{
				private:

					Tx::Sink *_tx_sink;
					Driver   &_driver;

				public:

					Tx_thread(Tx::Sink *tx_sink, Driver &driver)
					:
						Genode::Thread<TX_STACK_SIZE>("tx"),
						_tx_sink(tx_sink), _driver(driver)
					{
						start();
					}

					void entry()
					{
						using namespace Genode;

						while (true) {

							/* block for packet from client */
							Packet_descriptor packet = _tx_sink->get_packet();
							if (!packet.valid()) {
								PWRN("received invalid packet");
								continue;
							}

							_driver.tx(_tx_sink->packet_content(packet),
							           packet.size());

							/* acknowledge packet to the client */
							if (!_tx_sink->ready_to_ack())
								PDBG("need to wait until ready-for-ack");
							_tx_sink->acknowledge_packet(packet);
						}
					}
			} _tx_thread;

			void dump()
			{
				using namespace Genode;

				if (!VERBOSE_RX) return;

				char  *buf  = (char *)_rx.source()->packet_content(_curr_rx_packet);
				size_t size = _curr_rx_packet.size();

				printf("rx packet:");
				for (unsigned i = 0; i < size; i++)
					printf("%02x,", buf[i]);
				printf("\n");
			}

		public:

			/**
			 * Constructor
			 *
			 * \param tx_buf_size        buffer size for tx channel
			 * \param rx_buf_size        buffer size for rx channel
			 * \param rx_block_alloc     rx block allocator
			 * \param ep                 entry point used for packet stream
			 */
			Session_component(Genode::size_t          tx_buf_size,
			                  Genode::size_t          rx_buf_size,
			                  Nic::Driver_factory    &driver_factory,
			                  Genode::Rpc_entrypoint &ep)
			:
				Genode::Allocator_avl(Genode::env()->heap()),
				Session_rpc_object(Genode::env()->ram_session()->alloc(tx_buf_size),
				                   Genode::env()->ram_session()->alloc(rx_buf_size),
				                   static_cast<Genode::Range_allocator *>(this), ep),
				_driver_factory(driver_factory),
				_driver(*driver_factory.create(*this)),
				_tx_thread(_tx.sink(), _driver)
			{ }

			/**
			 * Destructor
			 */
			~Session_component()
			{
				_driver_factory.destroy(&_driver);
			}


			/*******************************
			 ** Rx_buffer_alloc interface **
			 *******************************/

			void *alloc(Genode::size_t size)
			{
				/* assign rx packet descriptor */
				_curr_rx_packet = _rx.source()->alloc_packet(size);

				return _rx.source()->packet_content(_curr_rx_packet);
			}

			void submit()
			{
				/* check for acknowledgements from the client */
				while (_rx.source()->ack_avail()) {
					Packet_descriptor packet = _rx.source()->get_acked_packet();

					/* free packet buffer */
					_rx.source()->release_packet(packet);
				}

				dump();

				_rx.source()->submit_packet(_curr_rx_packet);

				/* invalidate rx packet descriptor */
				_curr_rx_packet = Packet_descriptor();
			}


			/****************************
			 ** Nic::Session interface **
			 ****************************/

			Mac_address mac_address() { return _driver.mac_address(); }
			Tx::Sink*   tx_sink()     { return _tx.sink();   }
			Rx::Source* rx_source()   { return _rx.source(); }
	};

	/**
	 * Shortcut for single-client root component
	 */
	typedef Genode::Root_component<Session_component, Genode::Single_client> Root_component;

	/*
	 * Root component, handling new session requests.
	 */
	class Root : public Root_component
	{
		private:

			Driver_factory         &_driver_factory;
			Genode::Rpc_entrypoint &_ep;

		protected:

			/*
			 * Always returns the singleton nic-session component.
			 */
			Session_component *_create_session(const char *args)
			{
				using namespace Genode;

				Genode::size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				Genode::size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
				Genode::size_t rx_buf_size =
					Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

				/* delete ram quota by the memory needed for the session */
				Genode::size_t session_size = max((Genode::size_t)4096, sizeof(Session_component)
				                                  + sizeof(Allocator_avl));
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				/*
				 * Check if donated ram quota suffices for both
				 * communication buffers. Also check both sizes separately
				 * to handle a possible overflow of the sum of both sizes.
				 */
				if (tx_buf_size                  > ram_quota - session_size
					|| rx_buf_size               > ram_quota - session_size
					|| tx_buf_size + rx_buf_size > ram_quota - session_size) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, tx_buf_size + rx_buf_size + session_size);
					throw Root::Quota_exceeded();
				}

				return new (md_alloc()) Session_component(tx_buf_size,
				                                          rx_buf_size,
				                                          _driver_factory,
				                                          _ep);
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc,
			     Nic::Driver_factory    &driver_factory)
			:
				Root_component(session_ep, md_alloc),
				_driver_factory(driver_factory),
				_ep(*session_ep)
			{ }
	};
};

#endif /* _INCLUDE__NIC__COMPONENT_H_ */
