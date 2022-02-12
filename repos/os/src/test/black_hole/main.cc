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

/* os includes */
#include <event_session/connection.h>
#include <capture_session/connection.h>
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>
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
		};

		Env                         &_env;
		Heap                         _heap                { _env.ram(), _env.rm() };
		Event::Connection            _event               { _env };
		Capture::Connection          _capture             { _env };
		Capture::Area                _capture_screen_size { 1, 1 };
		Capture::Pixel               _capture_pixels[1];
		Surface<Capture::Pixel>      _capture_surface     { _capture_pixels, _capture_screen_size };
		Capture::Connection::Screen  _capture_screen      { _capture, _env.rm(), _capture_screen_size };
		Audio_in::Connection         _audio_in            { _env, "left" };
		Audio_out::Connection        _audio_out           { _env, "left" };
		Allocator_avl                _nic_tx_blk_alloc    { &_heap };
		Nic::Connection              _nic                 { _env, &_nic_tx_blk_alloc, NIC_BUF_SIZE, NIC_BUF_SIZE };
		Signal_handler<Main>         _nic_handler         { _env.ep(), *this, &Main::_handle_nic };
		unsigned long                _nic_pkt_count       { 0 };

		void _check_if_test_has_finished()
		{
			if (_nic_pkt_count > 100) {
				log("Finished");
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
				_nic_pkt_count++;
			}
			_submit_nic_pkts();
			_check_if_test_has_finished();
		}

		void _submit_nic_pkts()
		{
			for (unsigned idx { 0 }; idx < 40; idx++) {

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
