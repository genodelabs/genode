/*
 * \brief  Simple loop-back pseudo network adaptor
 * \author Norman Feske
 * \date   2009-11-13
 *
 * This program showcases the server-side use of the 'Nic_session' interface.
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/heap.h>
#include <root/component.h>
#include <util/arg_string.h>
#include <util/misc_math.h>
#include <nic/component.h>
#include <nic/packet_allocator.h>

namespace Nic_loopback {
	class Session_component;
	class Root;
	class Main;

	using namespace Genode;
}


class Nic_loopback::Session_component : public Nic::Session_component
{
	public:

		/**
		 * Constructor
		 *
		 * \param tx_buf_size        buffer size for tx channel
		 * \param rx_buf_size        buffer size for rx channel
		 * \param rx_block_md_alloc  backing store of the meta data of the
		 *                           rx block allocator
		 */
		Session_component(size_t const tx_buf_size,
		                  size_t const rx_buf_size,
		                  Allocator   &rx_block_md_alloc,
		                  Env         &env)
		:
			Nic::Session_component(tx_buf_size, rx_buf_size, CACHED,
			                       rx_block_md_alloc, env)
		{ }

		Nic::Mac_address mac_address() override
		{
			char buf[] = {1,2,3,4,5,6};
			Nic::Mac_address result((void*)buf);
			return result;
		}

		bool link_state() override
		{
			/* XXX always return true, for now */
			return true;
		}

		void _handle_packet_stream() override;
};


void Nic_loopback::Session_component::_handle_packet_stream()
{
	size_t const alloc_size = Nic::Packet_allocator::DEFAULT_PACKET_SIZE;

	/* loop while we can make progress */
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


		Packet_descriptor packet_to_client;
		try {
			packet_to_client = _rx.source()->alloc_packet(alloc_size); }
		catch (Session::Rx::Source::Packet_alloc_failed) {
			continue; }

		/* obtain packet */
		Packet_descriptor const packet_from_client = _tx.sink()->get_packet();
		if (!packet_from_client.size() || !_tx.sink()->packet_valid(packet_from_client)) {
			warning("received invalid packet");
			_rx.source()->release_packet(packet_to_client);
			continue;
		}

		memcpy(_rx.source()->packet_content(packet_to_client),
		       _tx.sink()->packet_content(packet_from_client),
		       packet_from_client.size());

		packet_to_client = Packet_descriptor(packet_to_client.offset(),
		                                     packet_from_client.size());
		_rx.source()->submit_packet(packet_to_client);

		_tx.sink()->acknowledge_packet(packet_from_client);
	}
}


class Nic_loopback::Root : public Root_component<Session_component>
{
	private:

		Env  &_env;

	protected:

		Session_component *_create_session(char const *args) override
		{
			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);
			size_t rx_buf_size = Arg_string::find_arg(args, "rx_buf_size").ulong_value(0);

			/* deplete ram quota by the memory needed for the session structure */
			size_t session_size = max(4096UL, (size_t)sizeof(Session_component));
			if (ram_quota < session_size)
				throw Insufficient_ram_quota();

			/*
			 * Check if donated ram quota suffices for both communication
			 * buffers and check for overflow
			 */
			if (tx_buf_size + rx_buf_size < tx_buf_size ||
			    tx_buf_size + rx_buf_size > ram_quota - session_size) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
				      "need ", tx_buf_size + rx_buf_size + session_size);
				throw Insufficient_ram_quota();
			}

			return new (md_alloc()) Session_component(tx_buf_size, rx_buf_size,
			                                          *md_alloc(), _env);
		}

	public:

		Root(Env       &env,
		     Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};


struct Nic_loopback::Main
{
	Env &_env;

	Heap _heap { _env.ram(), _env.rm() };

	Nic_loopback::Root _root { _env, _heap };

	Main(Env &env) : _env(env)
	{
		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env) { static Nic_loopback::Main main(env); }

