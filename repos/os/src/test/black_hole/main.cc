/*
 * \brief  Testing the functionality of the black hole component
 * \author Martin Stein
 * \date   2022-02-11
 *
 * FIXME
 *
 * Accessing the Audio_in, Audio_out and Usb connections is yet missing.
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
#include <rom_session/connection.h>
#include <base/attached_rom_dataspace.h>

/* os includes */
#include <gpu_session/connection.h>
#include <event_session/connection.h>
#include <usb_session/connection.h>
#include <capture_session/connection.h>
#include <audio_in_session/connection.h>
#include <audio_out_session/connection.h>
#include <uplink_session/connection.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <input/keycodes.h>
#include <timer_session/connection.h>

using namespace Genode;

namespace Black_hole_test {

	class Nic_test;
	class Uplink_test;
	class Capture_test;
	class Event_test;
	class Usb_test;
	class Rom_test;
	class Main;
}


class Black_hole_test::Nic_test
{
	private:

		enum {
			BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128,
			PKT_SIZE = 100,
		};

		Env                       &_env;
		Allocator                 &_alloc;
		Signal_context_capability  _sigh;
		Nic::Packet_allocator      _packet_alloc     { &_alloc };
		Nic::Connection            _connection       { _env, &_packet_alloc, BUF_SIZE, BUF_SIZE };
		unsigned long              _nr_of_sent_pkts  { 0 };
		unsigned long              _nr_of_acked_pkts { 0 };

		void _submit_pkts()
		{
			for (; _nr_of_sent_pkts < 40; _nr_of_sent_pkts++) {

				if (!_connection.tx()->ready_to_submit()) {
					class Submit_queue_full { };
					throw Submit_queue_full { };
				}
				Packet_descriptor pkt;
				try { pkt = _connection.tx()->alloc_packet(PKT_SIZE); }
				catch (...) {
					class Packet_alloc_failed { };
					throw Packet_alloc_failed { };
				}
				_connection.tx()->submit_packet(pkt);
			}
		}

	public:

		Nic_test(Env                       &env,
		         Allocator                 &alloc,
		         Signal_context_capability  sigh)
		:
			_env   { env },
			_alloc { alloc },
			_sigh  { sigh }
		{
			_connection.tx_channel()->sigh_ready_to_submit(_sigh);
			_connection.tx_channel()->sigh_ack_avail(_sigh);
			_connection.rx_channel()->sigh_ready_to_ack(_sigh);
			_connection.rx_channel()->sigh_packet_avail(_sigh);

			if (!_connection.link_state()) {
				class Link_down { };
				throw Link_down { };
			}
			if (_connection.mac_address() == Net::Mac_address { }) {
				class Mac_invalid { };
				throw Mac_invalid { };
			}
			_submit_pkts();
		}

		void handle_signal()
		{
			if (_connection.rx()->packet_avail()) {
				class Rx_packet_avail { };
				throw Rx_packet_avail { };
			}
			if (!_connection.rx()->ack_slots_free()) {
				class No_rx_ack_slots_free { };
				throw No_rx_ack_slots_free { };
			}
			while (_connection.tx()->ack_avail()) {

				Packet_descriptor const pkt {
					_connection.tx()->get_acked_packet() };

				if (pkt.size() != PKT_SIZE) {
					class Packet_size_unexpected { };
					throw Packet_size_unexpected { };
				}
				_connection.tx()->release_packet(pkt);
				_nr_of_sent_pkts--;
				_nr_of_acked_pkts++;
			}
			_submit_pkts();
		}

		bool finished() const
		{
			return _nr_of_acked_pkts > 100;
		}
};


class Black_hole_test::Uplink_test
{
	private:

		enum {
			BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128,
			PKT_SIZE = 100,
		};

		Env                                  &_env;
		Allocator                            &_alloc;
		Timer::Connection                    &_timer;
		Signal_context_capability             _sigh;
		Timer::Periodic_timeout<Uplink_test>  _timeout                  { _timer, *this, &Uplink_test::_execute_link_down_up_step, Microseconds { 1000000 } };
		Constructible<Nic::Packet_allocator>  _packet_alloc             { };
		Constructible<Uplink::Connection>     _connection               { };
		unsigned long                         _nr_of_sent_pkts          { 0 };
		unsigned long                         _nr_of_acked_pkts         { 0 };
		unsigned long                         _nr_of_link_down_up_steps { 0 };

		void _execute_link_down_up_step(Duration)
		{
			_connection.destruct();
			_packet_alloc.destruct();
			_nr_of_sent_pkts = 0;

			_packet_alloc.construct(&_alloc);
			_connection.construct(
				_env, &(*_packet_alloc), BUF_SIZE,
				BUF_SIZE, Net::Mac_address { 2 });

			_connection->tx_channel()->sigh_ready_to_submit(_sigh);
			_connection->tx_channel()->sigh_ack_avail(_sigh);
			_connection->rx_channel()->sigh_ready_to_ack(_sigh);
			_connection->rx_channel()->sigh_packet_avail(_sigh);
			_submit_pkts();

			_nr_of_link_down_up_steps++;
		}

		void _submit_pkts()
		{
			for (; _nr_of_sent_pkts < 30; _nr_of_sent_pkts++) {

				if (!_connection->tx()->ready_to_submit()) {
					class Submit_queue_full { };
					throw Submit_queue_full { };
				}
				Packet_descriptor pkt;
				try { pkt = _connection->tx()->alloc_packet(PKT_SIZE); }
				catch (...) {
					class Packet_alloc_failed { };
					throw Packet_alloc_failed { };
				}
				_connection->tx()->submit_packet(pkt);
			}
		}

	public:

		Uplink_test(Env                       &env,
		            Allocator                 &alloc,
		            Timer::Connection         &timer,
		            Signal_context_capability  sigh)
		:
			_env   { env },
			_alloc { alloc },
			_timer { timer },
			_sigh  { sigh }
		{ }

		void handle_signal()
		{
			if (!_connection.constructed()) {
				return;
			}
			if (_connection->rx()->packet_avail()) {
				class Rx_packet_avail { };
				throw Rx_packet_avail { };
			}
			if (!_connection->rx()->ack_slots_free()) {
				class No_rx_ack_slots_free { };
				throw No_rx_ack_slots_free { };
			}
			while (_connection->tx()->ack_avail()) {

				Packet_descriptor const pkt {
					_connection->tx()->get_acked_packet() };

				if (pkt.size() != PKT_SIZE) {
					class Packet_size_unexpected { };
					throw Packet_size_unexpected { };
				}
				_connection->tx()->release_packet(pkt);
				_nr_of_sent_pkts--;
				_nr_of_acked_pkts++;
			}
			_submit_pkts();
		}

		bool finished() const
		{
			return
				_nr_of_acked_pkts > 200 &&
				_nr_of_link_down_up_steps > 2;
		}
};


class Black_hole_test::Capture_test
{
	private:

		Env                         &_env;
		Capture::Connection          _connection  { _env };
		Capture::Area                _screen_size { 1, 1 };
		Capture::Pixel               _pixels[1];
		Surface<Capture::Pixel>      _surface     { _pixels, _screen_size };
		Capture::Connection::Screen  _screen      { _connection, _env.rm(), _screen_size };
		bool                         _finished    { false };

	public:

		Capture_test(Env &env)
		:
			_env { env }
		{
			_screen.apply_to_surface(_surface);
			_finished = true;
		}

		bool finished() const
		{
			return _finished;
		}
};


class Black_hole_test::Event_test
{
	private:

		Env               &_env;
		Event::Connection  _connection { _env };
		bool               _finished   { false };

	public:

		Event_test(Env &env)
		:
			_env { env }
		{
			_connection.with_batch([&] (Event::Session_client::Batch &batch) {
				batch.submit(Input::Press {Input::KEY_1 });
				batch.submit(Input::Release {Input::KEY_2 });
				batch.submit(Input::Relative_motion { 3, 4 });
			});
			_finished = true;
		}

		bool finished() const
		{
			return _finished;
		}
};


class Black_hole_test::Rom_test
{
	private:

		Env                    &_env;
		Attached_rom_dataspace  _rom_ds   { _env, "any_label" };
		bool                    _finished { false };

	public:

		Rom_test(Env &env)
		:
			_env { env }
		{
			String<16> const str { Cstring { _rom_ds.local_addr<char>() } };
			if (str != "<empty/>") {
				class Unexpected_rom_content { };
				throw Unexpected_rom_content { };
			}
			_finished = true;
		}

		bool finished() const
		{
			return _finished;
		}
};


class Black_hole_test::Usb_test
{
	private:

		Env             &_env;
		Allocator_avl    _alloc;
		Usb::Connection  _connection { _env  };
		bool             _finished   { false };

	public:

		Usb_test(Env  &env,
		         Heap &heap)
		:
			_env   { env },
			_alloc { &heap }
		{
			_finished = true;
		}

		bool finished() const
		{
			return _finished;
		}
};



class Black_hole_test::Main
{
	private:

		Env                   &_env;
		Timer::Connection      _timer          { _env };
		Heap                   _heap           { _env.ram(), _env.rm() };
		Signal_handler<Main>   _signal_handler { _env.ep(), *this, &Main::_handle_signal };
		Audio_in::Connection   _audio_in       { _env, "left" };
		Audio_out::Connection  _audio_out      { _env, "left" };
		Gpu::Connection        _gpu            { _env };
		Nic_test               _nic_test       { _env, _heap, _signal_handler };
		Uplink_test            _uplink_test    { _env, _heap, _timer, _signal_handler };
		Capture_test           _capture_test   { _env };
		Event_test             _event_test     { _env };
		Usb_test               _usb_test       { _env, _heap };
		Rom_test               _rom_test       { _env };

		void _handle_signal()
		{
			_nic_test.handle_signal();
			_uplink_test.handle_signal();
			_check_if_tests_have_finished();
		}

		void _check_if_tests_have_finished()
		{
			if (_nic_test.finished() &&
			    _uplink_test.finished() &&
			    _capture_test.finished() &&
			    _event_test.finished() &&
			    _usb_test.finished() &&
			    _rom_test.finished()) {

				log("Finished");
			}
		}

	public:

		Main(Env &env)
		:
			_env { env }
		{
			_check_if_tests_have_finished();
		}
};


void Component::construct(Env &env)
{
	static Black_hole_test::Main main { env };
}
