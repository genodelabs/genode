/*
 * \brief  C interface to Genode's uplink session
 * \author Norman Feske
 * \date   2021-07-06
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <base/registry.h>
#include <base/log.h>
#include <base/session_label.h>
#include <uplink_session/connection.h>
#include <nic/packet_allocator.h>
#include <genode_c_api/uplink.h>

using namespace Genode;

static Env                                *_env_ptr;
static Allocator                          *_alloc_ptr;
static Signal_context_capability           _sigh { };
static Registry<Registered<genode_uplink>> _uplinks { };


struct genode_uplink : private Noncopyable, private Interface
{
	private:

		Env       &_env;
		Allocator &_alloc;

		Nic::Packet_allocator _packet_alloc { &_alloc };

		static constexpr size_t
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = Uplink::Session::QUEUE_SIZE * PACKET_SIZE;

		Net::Mac_address const _mac_address;

		Session_label const _session_label;

		Uplink::Connection _connection { _env, &_packet_alloc,
		                                 BUF_SIZE, BUF_SIZE,
		                                 _mac_address,
		                                 _session_label.string() };

	public:

		genode_uplink(Env &env, Allocator &alloc, Signal_context_capability sigh,
		              Net::Mac_address const &mac_address,
		              Session_label    const &session_label)
		:
			_env(env), _alloc(alloc),
			_mac_address(mac_address),
			_session_label(session_label)
		{
			_connection.rx_channel()->sigh_ready_to_ack   (sigh);
			_connection.rx_channel()->sigh_packet_avail   (sigh);
			_connection.tx_channel()->sigh_ack_avail      (sigh);
			_connection.tx_channel()->sigh_ready_to_submit(sigh);

			/* trigger signal handling once after construction */
			Signal_transmitter(sigh).submit();
		}

		void notify_peer()
		{
			_connection.rx()->wakeup();
			_connection.tx()->wakeup();
		}

		template <typename FN>
		bool tx_one_packet(FN const &fn)
		{
			bool progress = false;

			Uplink::Session::Tx::Source &tx_source = *_connection.tx();

			/*
			 * Process acknowledgements
			 */

			while (tx_source.ack_avail()) {
				tx_source.release_packet(tx_source.try_get_acked_packet());
				progress = true;
			}

			/*
			 * Submit packet
			 */

			if (!tx_source.ready_to_submit(1))
				return progress;

			typedef Uplink::Packet_descriptor Packet_descriptor;

			Packet_descriptor packet { };
			size_t const max_bytes = Nic::Packet_allocator::OFFSET_PACKET_SIZE;

			try { packet = tx_source.alloc_packet(max_bytes); }
			catch (Uplink::Session::Tx::Source::Packet_alloc_failed) {
				return progress; /* packet-stream buffer is saturated */ }

			char * const dst_ptr = tx_source.packet_content(packet);
			size_t const payload_bytes = min(max_bytes, fn(dst_ptr, max_bytes));

			/* imprint payload size into packet descriptor */
			packet = Packet_descriptor(packet.offset(), payload_bytes);

			tx_source.try_submit_packet(packet);
			progress = true;

			return progress;
		}

		template <typename FN>
		bool for_each_rx_packet(FN const &fn)
		{
			/*
			 * Implementation mirrored from (commented) block/request_stream.h
			 */
			bool overall_progress = false;

			Uplink::Session::Rx::Sink &rx_sink = *_connection.rx();

			for (;;) {

				if (!rx_sink.packet_avail() || !rx_sink.ack_slots_free())
					break;

				typedef Uplink::Packet_descriptor Packet_descriptor;

				Packet_descriptor const packet = rx_sink.peek_packet();

				bool const packet_valid = rx_sink.packet_valid(packet)
				                       && (packet.offset() >= 0);

				char const *content = rx_sink.packet_content(packet);

				genode_uplink_rx_result_t const
					response = packet_valid
					         ? fn(content, packet.size())
					         : GENODE_UPLINK_RX_REJECTED;

				bool progress = false;

				switch (response) {

				case GENODE_UPLINK_RX_ACCEPTED:
				case GENODE_UPLINK_RX_REJECTED:

					(void)rx_sink.try_get_packet();
					rx_sink.try_ack_packet(packet);
					progress = true;
					break;

				case GENODE_UPLINK_RX_RETRY:
					break;
				}

				if (progress)
					overall_progress = true;

				if (!progress)
					break;
			}
			return overall_progress;
		}
};


void genode_uplink_init(genode_env            *env_ptr,
                        genode_allocator      *alloc_ptr,
                        genode_signal_handler *sigh_ptr)
{
	_env_ptr   = env_ptr;
	_alloc_ptr = alloc_ptr;
	_sigh      = cap(sigh_ptr);
}


void genode_uplink_notify_peers()
{
	_uplinks.for_each([&] (genode_uplink &uplink) {
		uplink.notify_peer(); });
}


bool genode_uplink_tx_packet(struct genode_uplink *uplink_ptr,
                             genode_uplink_tx_packet_content_t tx_packet_content_cb,
                             struct genode_uplink_tx_packet_context *ctx_ptr)
{
	return uplink_ptr->tx_one_packet([&] (char *dst, size_t len) {
		return tx_packet_content_cb(ctx_ptr, dst, len); });
}


bool genode_uplink_rx(struct genode_uplink *uplink_ptr,
                      genode_uplink_rx_one_packet_t rx_one_packet_cb,
                      struct genode_uplink_rx_context *ctx_ptr)
{
	return uplink_ptr->for_each_rx_packet([&] (char const *ptr, size_t len) {
		return rx_one_packet_cb(ctx_ptr, ptr, len); });
}


struct genode_uplink *genode_uplink_create(struct genode_uplink_args const *args)
{
	if (!_env_ptr || !_alloc_ptr) {
		error("genode_uplink_create: missing call of genode_uplink_init");
		return nullptr;
	}

	Net::Mac_address mac { };
	for (unsigned i = 0; i < sizeof(args->mac_address); i++)
		mac.addr[i] = args->mac_address[i];

	return new (*_alloc_ptr)
		Registered<genode_uplink>(_uplinks, *_env_ptr, *_alloc_ptr, _sigh,
		                          mac, Session_label(args->label));
}


void genode_uplink_destroy(struct genode_uplink *uplink_ptr)
{
	destroy(*_alloc_ptr, uplink_ptr);
}
