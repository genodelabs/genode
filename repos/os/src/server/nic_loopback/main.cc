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
#include <os/attached_ram_dataspace.h>
#include <os/server.h>
#include <os/signal_rpc_dispatcher.h>
#include <root/component.h>
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <nic_session/rpc_object.h>
#include <nic_session/client.h>

namespace Nic {

	class Communication_buffers;
	class Session_component;
	class Root;
}


namespace Server { struct Main; }


class Nic::Communication_buffers
{
	protected:

		Genode::Allocator_avl _rx_packet_alloc;

		Genode::Attached_ram_dataspace _tx_ds, _rx_ds;

		Communication_buffers(Genode::Allocator &rx_block_md_alloc,
		                      Genode::Ram_session &ram_session,
		                      Genode::size_t tx_size, Genode::size_t rx_size)
		:
			_rx_packet_alloc(&rx_block_md_alloc),
			_tx_ds(&ram_session, tx_size),
			_rx_ds(&ram_session, rx_size)
		{ }
};


class Nic::Session_component : Communication_buffers, public Session_rpc_object
{
	private:

		Server::Entrypoint &_ep;

		void _handle_packet_stream(unsigned);

		Genode::Signal_rpc_member<Session_component> _packet_stream_dispatcher {
			_ep, *this, &Session_component::_handle_packet_stream };

	public:

		/**
		 * Constructor
		 *
		 * \param tx_buf_size        buffer size for tx channel
		 * \param rx_buf_size        buffer size for rx channel
		 * \param rx_block_md_alloc  backing store of the meta data of the
		 *                           rx block allocator
		 * \param ram_session        RAM session to allocate tx and rx buffers
		 * \param ep                 entry point used for packet stream
		 *                           channels
		 */
		Session_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator   &rx_block_md_alloc,
		                  Genode::Ram_session &ram_session,
		                  Server::Entrypoint  &ep)
		:
			Communication_buffers(rx_block_md_alloc, ram_session,
			                      tx_buf_size, rx_buf_size),
			Session_rpc_object(this->_tx_ds.cap(),
			                   this->_rx_ds.cap(),
			                   &this->_rx_packet_alloc, ep.rpc_ep()),
			_ep(ep)
		{
			/* install data-flow signal handlers for both packet streams */
			_tx.sigh_ready_to_ack(_packet_stream_dispatcher);
			_tx.sigh_packet_avail(_packet_stream_dispatcher);
			_rx.sigh_ready_to_submit(_packet_stream_dispatcher);
			_rx.sigh_ack_avail(_packet_stream_dispatcher);
		}

		Mac_address mac_address()
		{
			Mac_address result = {{1,2,3,4,5,6}};
			return result;
		}

		bool link_state()
		{
			/* XXX always return true, for now */
			return true;
		}

		void link_state_sigh(Genode::Signal_context_capability sigh) { }
};


void Nic::Session_component::_handle_packet_stream(unsigned)
{
	using namespace Genode;

	/* loop unless we cannot make any progress */
	for (;;) {

		/* flush acknowledgements for the echoes packets */
		while (_rx.source()->ack_avail())
			_rx.source()->release_packet(_rx.source()->get_acked_packet());

		/*
		 * If the client cannot accept new acknowledgements for a sent packets,
		 * we won't consume the sent packet.
		 */
		if (!_tx.sink()->ready_to_ack())
			return;

		/*
		 * Nothing to be done if the client has not sent any packets.
		 */
		if (!_tx.sink()->packet_avail())
			return;

		/*
		 * Here we know that the client has submitted a packet to us and is also
		 * able it receive the corresponding acknowledgement.
		 */

		/*
		 * The client fails to pick up the packets from the rx channel. So we
		 * won't try to submit new packets.
		 */
		if (!_rx.source()->ready_to_submit())
			return;

		/*
		 * We are safe to process one packet without blocking.
		 */

		/* obtain packet */
		Packet_descriptor const packet_from_client = _tx.sink()->get_packet();
		if (!packet_from_client.valid()) {
			PWRN("received invalid packet");
			continue;
		}

		try {
			size_t const packet_size = packet_from_client.size();

			Packet_descriptor const packet_to_client =
				_rx.source()->alloc_packet(packet_size);

			Genode::memcpy(_rx.source()->packet_content(packet_to_client),
			               _tx.sink()->packet_content(packet_from_client),
			               packet_size);

			_rx.source()->submit_packet(packet_to_client);

		} catch (Session::Rx::Source::Packet_alloc_failed) {
			PWRN("transmit packet allocation failed, drop packet");
		}

		_tx.sink()->acknowledge_packet(packet_from_client);
	}
}


class Nic::Root : public Genode::Root_component<Session_component>
{
	private:

		Server::Entrypoint &_ep;

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
			                                          *env()->heap(),
			                                          *env()->ram_session(),
			                                          _ep);
		}

	public:

		Root(Server::Entrypoint &ep, Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep)
		{ }
};


struct Server::Main
{
	Entrypoint &ep;

	Nic::Root nic_root { ep, *Genode::env()->heap() };

	Main(Entrypoint &ep) : ep(ep)
	{
		Genode::env()->parent()->announce(ep.manage(nic_root));
	}
};


namespace Server {

	char const *name() { return "nicloop_ep"; }

	size_t stack_size() { return 2*1024*sizeof(long); }

	void construct(Entrypoint &ep)
	{
		static Main main(ep);
	}
}
