/*
 * \brief  Simple loop-back pseudo network adaptor
 * \author Norman Feske
 * \date   2009-11-13
 *
 * This program showcases the server-side use of the 'Nic_session' interface.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/sleep.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <nic_session/rpc_object.h>
#include <nic_session/client.h>

#include <timer_session/connection.h>


namespace Nic {

	class Communication_buffer : Genode::Ram_dataspace_capability
	{
		public:

			Communication_buffer(Genode::size_t size)
			: Genode::Ram_dataspace_capability(Genode::env()->ram_session()->alloc(size))
			{ }

			~Communication_buffer() { Genode::env()->ram_session()->free(*this); }

			Genode::Dataspace_capability dataspace() { return *this; }
	};


	class Tx_rx_communication_buffers
	{
		private:

			Communication_buffer _tx_buf, _rx_buf;

		public:

			Tx_rx_communication_buffers(Genode::size_t tx_size,
			                            Genode::size_t rx_size)
			: _tx_buf(tx_size), _rx_buf(rx_size) { }

			Genode::Dataspace_capability tx_ds() { return _tx_buf.dataspace(); }
			Genode::Dataspace_capability rx_ds() { return _rx_buf.dataspace(); }
	};


	enum { STACK_SIZE = 8*1024 };
	class Session_component : private Genode::Allocator_avl,
	                          private Tx_rx_communication_buffers,
	                          private Genode::Thread<STACK_SIZE>,
	                          public  Session_rpc_object
	{
		private:

			/**
			 * Packet-handling thread
			 */
			void entry()
			{
				using namespace Genode;

				for (;;) {
					Packet_descriptor packet_from_client, packet_to_client;

					/* get packet, block until a packet is available */
					packet_from_client = _tx.sink()->get_packet();

					if (!packet_from_client.valid()) {
						PWRN("received invalid packet");
						continue;
					}

					size_t packet_size = packet_from_client.size();
					try {
						packet_to_client = _rx.source()->alloc_packet(packet_size);

						Genode::memcpy(_rx.source()->packet_content(packet_to_client),
						               _tx.sink()->packet_content(packet_from_client),
						               packet_size);

						_rx.source()->submit_packet(packet_to_client);

					} catch (Session::Rx::Source::Packet_alloc_failed) {
						PWRN("transmit packet allocation failed, drop packet");
					}

					if (!_tx.sink()->ready_to_ack())
						printf("need to wait until ready-for-ack\n");

					_tx.sink()->acknowledge_packet(packet_from_client);

					/* flush acknowledgements for the echoes packets */
					while (_rx.source()->ack_avail())
						_rx.source()->release_packet(_rx.source()->get_acked_packet());
				}
			}

		public:

			/**
			 * Constructor
			 *
			 * \param tx_buf_size        buffer size for tx channel
			 * \param rx_buf_size        buffer size for rx channel
			 * \param rx_block_md_alloc  backing store of the meta data of the
			 *                           rx block allocator
			 * \param ep                 entry point used for packet stream
			 *                           channels
			 */
			Session_component(Genode::size_t          tx_buf_size,
			                  Genode::size_t          rx_buf_size,
			                  Genode::Allocator      *rx_block_md_alloc,
			                  Genode::Rpc_entrypoint &ep)
			:
				Genode::Allocator_avl(rx_block_md_alloc),
				Tx_rx_communication_buffers(tx_buf_size, rx_buf_size),
				Thread("nic_packet_handler"),
				Session_rpc_object(Tx_rx_communication_buffers::tx_ds(),
				                   Tx_rx_communication_buffers::rx_ds(),
				                   static_cast<Genode::Range_allocator *>(this), ep)
			{
				/* start packet-handling thread */
				start();
			}

			Mac_address mac_address()
			{
				Mac_address result = {{1,2,3,4,5,6}};
				return result;
			}
	};

	class Root : public Genode::Root_component<Session_component>
	{
		private:

			Genode::Rpc_entrypoint &_channel_ep;

		protected:

			Session_component *_create_session(const char *args)
			{
				using namespace Genode;

				size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
				size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

				/* deplete ram quota by the memory needed for the session structure */
				size_t session_size = max(4096UL, (unsigned long)sizeof(Session_component));
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				/*
				 * Check if donated ram quota suffices for both communication
				 * buffers. Also check both sizes separately to handle a
				 * possible overflow of the sum of both sizes.
				 */
				if (tx_buf_size               > ram_quota - session_size
				 || rx_buf_size               > ram_quota - session_size
				 || tx_buf_size + rx_buf_size > ram_quota - session_size) {
					PERR("insufficient 'ram_quota', got %zd, need %zd",
					     ram_quota, tx_buf_size + rx_buf_size + session_size);
					throw Root::Quota_exceeded();
				}

				return new (md_alloc()) Session_component(tx_buf_size, rx_buf_size,
				                                          env()->heap(), _channel_ep);
			}

		public:

			Root(Genode::Rpc_entrypoint *session_ep,
			     Genode::Allocator      *md_alloc)
			:
				Genode::Root_component<Session_component>(session_ep, md_alloc),
				_channel_ep(*session_ep)
			{ }
	};
}


using namespace Genode;

int main(int, char **)
{
	enum { STACK_SIZE = 2*4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nicloop_ep");

	static Nic::Root nic_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&nic_root));

	sleep_forever();
	return 0;
}

