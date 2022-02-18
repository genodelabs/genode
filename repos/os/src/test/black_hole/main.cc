/*
 * \brief  Testing the functionality of the black hole component
 * \author Martin Stein
 * \date   2022-02-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* base includes */
#include <base/component.h>
#include <base/heap.h>
#include <util/reconstructible.h>

/* os includes */
#include <event_session/connection.h>
#include <capture_session/connection.h>
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>
#include <uplink_session/connection.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <base/allocator_avl.h>
#include <input/keycodes.h>

using namespace Genode;

namespace Test {

	class Main;
}


class Test::Main
{
	private:

		enum {
			NIC_BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128,
			NIC_PKT_SIZE = 100,
			UPLINK_BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128,
			UPLINK_PKT_SIZE = 100,
		};

		Env                               &_env;
		Heap                               _heap                    { _env.ram(), _env.rm() };
		Event::Connection                  _event                   { _env };
		Capture::Connection                _capture                 { _env };
		Capture::Area                      _capture_screen_size     { 1, 1 };
		Capture::Pixel                     _capture_pixels[1];
		Surface<Capture::Pixel>            _capture_surface         { _capture_pixels, _capture_screen_size };
		Capture::Connection::Screen        _capture_screen          { _capture, _env.rm(), _capture_screen_size };
		Audio_in::Connection               _audio_in                { _env, "left" };
		Audio_out::Connection              _audio_out               { _env, "left" };
		Allocator_avl                      _nic_tx_blk_alloc        { &_heap };
		Nic::Connection                    _nic                     { _env, &_nic_tx_blk_alloc, NIC_BUF_SIZE, NIC_BUF_SIZE };
		Signal_handler<Main>               _nic_handler             { _env.ep(), *this, &Main::_handle_nic };
		unsigned long                      _nic_nr_of_sent_pkts     { 0 };
		unsigned long                      _nic_nr_of_acked_pkts    { 0 };
		Allocator_avl                      _uplink_tx_blk_alloc     { &_heap };
		Constructible<Uplink::Connection>  _uplink                  { };
		Signal_handler<Main>               _uplink_handler          { _env.ep(), *this, &Main::_handle_uplink };
		unsigned long                      _uplink_nr_of_sent_pkts  { 0 };
		unsigned long                      _uplink_nr_of_acked_pkts { 0 };

		void _check_if_test_has_finished()
		{
			if (_nic_nr_of_acked_pkts    > 100 &&
			    _uplink_nr_of_acked_pkts > 100) {

				log("Finished");
			}
		}

		void _reconstruct_uplink()
		{
			_uplink.destruct();
			_uplink.construct(
				_env, &_uplink_tx_blk_alloc, UPLINK_BUF_SIZE,
				UPLINK_BUF_SIZE, Net::Mac_address { 2 });

			_uplink->tx_channel()->sigh_ready_to_submit(_uplink_handler);
			_uplink->tx_channel()->sigh_ack_avail(_uplink_handler);
			_uplink->rx_channel()->sigh_ready_to_ack(_uplink_handler);
			_uplink->rx_channel()->sigh_packet_avail(_uplink_handler);
		}

		void _handle_uplink()
		{
			if (!_uplink.constructed()) {
				return;
			}
			if (_uplink->rx()->packet_avail()) {
				class Uplink_rx_packet_avail { };
				throw Uplink_rx_packet_avail { };
			}
			if (!_uplink->rx()->ack_slots_free()) {
				class Uplink_no_rx_ack_slots_free { };
				throw Uplink_no_rx_ack_slots_free { };
			}
			while (_uplink->tx()->ack_avail()) {

				Packet_descriptor const pkt { _uplink->tx()->get_acked_packet() };
				if (pkt.size() != UPLINK_PKT_SIZE) {
					class Uplink_packet_size_unexpected { };
					throw Uplink_packet_size_unexpected { };
				}
				_uplink->tx()->release_packet(pkt);
				_uplink_nr_of_sent_pkts--;
				_uplink_nr_of_acked_pkts++;
			}
			_submit_uplink_pkts();
			_check_if_test_has_finished();
		}

		void _submit_uplink_pkts()
		{
			for (; _uplink_nr_of_sent_pkts < 40; _uplink_nr_of_sent_pkts++) {

				if (!_uplink->tx()->ready_to_submit()) {
					class Uplink_submit_queue_full { };
					throw Uplink_submit_queue_full { };
				}
				Packet_descriptor pkt;
				try { pkt = _uplink->tx()->alloc_packet(UPLINK_PKT_SIZE); }
				catch (...) {
					class Uplink_packet_alloc_failed { };
					throw Uplink_packet_alloc_failed { };
				}
				_uplink->tx()->submit_packet(pkt);
			}
		}

		void _handle_nic()
		{
			if (_nic.rx()->packet_avail()) {
				class Nic_rx_packet_avail { };
				throw Nic_rx_packet_avail { };
			}
			if (!_nic.rx()->ack_slots_free()) {
				class Nic_no_rx_ack_slots_free { };
				throw Nic_no_rx_ack_slots_free { };
			}
			while (_nic.tx()->ack_avail()) {

				Packet_descriptor const pkt { _nic.tx()->get_acked_packet() };
				if (pkt.size() != NIC_PKT_SIZE) {
					class Nic_packet_size_unexpected { };
					throw Nic_packet_size_unexpected { };
				}
				_nic.tx()->release_packet(pkt);
				_nic_nr_of_sent_pkts--;
				_nic_nr_of_acked_pkts++;
			}
			_submit_nic_pkts();
			_check_if_test_has_finished();
		}

		void _submit_nic_pkts()
		{
			for (; _nic_nr_of_sent_pkts < 40; _nic_nr_of_sent_pkts++) {

				if (!_nic.tx()->ready_to_submit()) {
					class Nic_submit_queue_full { };
					throw Nic_submit_queue_full { };
				}
				Packet_descriptor pkt;
				try { pkt = _nic.tx()->alloc_packet(NIC_PKT_SIZE); }
				catch (...) {
					class Nic_packet_alloc_failed { };
					throw Nic_packet_alloc_failed { };
				}
				_nic.tx()->submit_packet(pkt);
			}
		}

	public:

		Main(Env &env) : _env { env }
		{
			/* test-drive uplink connection */
			_reconstruct_uplink();
			_submit_uplink_pkts();

			/* test-drive nic connection */
			_nic.tx_channel()->sigh_ready_to_submit(_nic_handler);
			_nic.tx_channel()->sigh_ack_avail(_nic_handler);
			_nic.rx_channel()->sigh_ready_to_ack(_nic_handler);
			_nic.rx_channel()->sigh_packet_avail(_nic_handler);
			if (!_nic.link_state()) {
				class Nic_link_down { };
				throw Nic_link_down { };
			}
			if (_nic.mac_address() == Net::Mac_address { }) {
				class Nic_mac_invalid { };
				throw Nic_mac_invalid { };
			}
			_submit_nic_pkts();

			/* test-drive event connection */
			_event.with_batch([&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Press {Input::KEY_1 });
				batch.submit(Input::Release {Input::KEY_2 });
				batch.submit(Input::Relative_motion { 3, 4 });
			});
			/* test-drive capture connection */
			_capture_screen.apply_to_surface(_capture_surface);

			/*
			 * FIXME
			 *
			 * Test-driving audio_in and audio_out connection is yet missing.
			 */
		}
};


void Component::construct(Env &env)
{
	static Test::Main main { env };
}
