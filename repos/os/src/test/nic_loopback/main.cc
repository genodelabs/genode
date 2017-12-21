/*
 * \brief  Test for the NIC loop-back service
 * \author Norman Feske
 * \date   2009-11-14
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <base/heap.h>
#include <base/allocator_avl.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

namespace Test {
	struct Base;
	struct Roundtrip;
	struct Batch;
	struct Main;

	using namespace Genode;
}


namespace Genode {

	static void print(Output &out, Packet_descriptor const &packet)
	{
		::Genode::print(out, "offset=", packet.offset(), ", size=", packet.size());
	}
}


struct Test::Base : Interface
{
	public:

		typedef String<64> Name;

		virtual void handle_nic() = 0;

	private:

		Env &_env;

		Name const _name;

		Signal_context_capability _succeeded_sigh;

		bool _done = false;

		Heap _heap { _env.ram(), _env.rm() };

		Allocator_avl _tx_block_alloc { &_heap };

		enum { BUF_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

		Nic::Connection _nic { _env, &_tx_block_alloc, BUF_SIZE, BUF_SIZE };

		void _handle_nic() { if (!_done) handle_nic(); }

		Signal_handler<Base> _nic_handler { _env.ep(), *this, &Base::_handle_nic };

	public:

		Nic::Connection &nic() { return _nic; }

		void success()
		{
			/*
			 * There may still be packet-stream signals in flight, which we can
			 * ignore once the test succeeded.
			 */
			_done = true;

			log("-- ", _name, " test succeeded --");

			Signal_transmitter(_succeeded_sigh).submit();
		}

		template <typename... ARGS>
		static void abort(ARGS &&... args)
		{
			error(args...);
			class Error : Exception { };
			throw Error();
		}

		Base(Env &env, Name const &name, Signal_context_capability succeeded_sigh)
		:
			_env(env), _name(name), _succeeded_sigh(succeeded_sigh)
		{
			log("-- starting ", _name, " test --");

			_nic.tx_channel()->sigh_ready_to_submit(_nic_handler);
			_nic.tx_channel()->sigh_ack_avail      (_nic_handler);
			_nic.rx_channel()->sigh_ready_to_ack   (_nic_handler);
			_nic.rx_channel()->sigh_packet_avail   (_nic_handler);
		}
};


struct Test::Roundtrip : Base
{
	/*
	 * Each character of the string is used as pattern for one iteration.
	 */
	typedef String<16> Patterns;

	Patterns const _patterns;

	unsigned _cnt = 0;

	off_t _expected_packet_offset = ~0L;

	char _pattern() const { return _patterns.string()[_cnt]; }

	bool _received_acknowledgement  = false;
	bool _received_reflected_packet = false;

	enum { PACKET_SIZE = 100 };

	void _produce_packet(Nic::Connection &nic)
	{
		log("start iteration ", _cnt, " with pattern '", Char(_pattern()), "'");

		Packet_descriptor tx_packet;

		/* allocate tx packet */
		try {
			tx_packet = nic.tx()->alloc_packet(PACKET_SIZE); }
		catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
			abort(__func__, ": tx packet alloc failed"); }

		/*
		 * Remember the packet offset of the first packet. The offsets
		 * of all subsequent packets are expected to be the same.
		 */
		if (_expected_packet_offset == ~0L)
			_expected_packet_offset = tx_packet.offset();

		log("allocated tx packet ", tx_packet);

		/* fill packet with pattern */
		char *tx_content = nic.tx()->packet_content(tx_packet);
		for (unsigned i = 0; i < PACKET_SIZE; i++)
			tx_content[i] = _pattern();

		if (!nic.tx()->ready_to_submit())
			abort(__func__, ": submit queue is unexpectedly full");

		nic.tx()->submit_packet(tx_packet);
	}

	void _consume_and_compare_packet(Nic::Connection &nic)
	{
		/*
		 * The acknowledgement for the sent packet and the reflected packet
		 * may arrive in any order.
		 */

		if (nic.tx()->ack_avail()) {

			/* wait for acknowledgement */
			Packet_descriptor const ack_tx_packet = nic.tx()->get_acked_packet();

			if (ack_tx_packet.size() != PACKET_SIZE)
				abort(__func__, ": unexpected acked packet");

			if (ack_tx_packet.offset() != _expected_packet_offset)
				abort(__func__, ": unexpected offset of acknowledged packet");

			/* release sent packet to free the space in the tx communication buffer */
			nic.tx()->release_packet(ack_tx_packet);

			_received_acknowledgement = true;
		}

		if (nic.rx()->packet_avail()) {

			/*
			 * Because we sent the packet to a loop-back device, we expect
			 * the same packet to be available at the rx channel of the NIC
			 * session.
			 */
			Packet_descriptor const rx_packet = nic.rx()->get_packet();
			log("received rx packet ", rx_packet);

			if (rx_packet.size() != PACKET_SIZE)
				abort("sent and echoed packets differ in size");

			if (rx_packet.offset() != _expected_packet_offset)
				abort(__func__, ": unexpected offset of received packet");

			/* compare original and echoed packets */
			char const * const rx_content = nic.rx()->packet_content(rx_packet);
			for (unsigned i = 0; i < PACKET_SIZE; i++)
				if (rx_content[i] != _pattern()) {
					log("rx_content[", i, "]: ", Char(rx_content[i]));
					log("pattern: ", Char(_pattern()));
					abort(__func__, ":sent and echoed packets have different content");
				}

			if (!nic.rx()->ack_slots_free())
				abort(__func__, ": acknowledgement queue is unexpectedly full");

			nic.rx()->acknowledge_packet(rx_packet);

			_received_reflected_packet = true;
		}
	}

	void handle_nic() override
	{
		_consume_and_compare_packet(nic());

		if (!_received_acknowledgement || !_received_reflected_packet)
			return;

		/* start next iteration */
		_cnt++;

		/* check if we reached the end of the pattern string */
		if (_pattern() == 0) {
			success();
			return;
		}

		_received_reflected_packet = false;
		_received_acknowledgement  = false;
		_produce_packet(nic());
	}

	Roundtrip(Env &env, Signal_context_capability success_sigh, Patterns patterns)
	:
		Base(env, "roundtrip", success_sigh), _patterns(patterns)
	{
		_produce_packet(nic());
	}
};


struct Test::Batch : Base
{
	unsigned const _num_packets;

	unsigned _tx_cnt = 0, _acked_cnt = 0, _rx_cnt = 0, _batch_cnt = 0;

	enum { PACKET_SIZE = 100 };

	static unsigned _send_packets(Nic::Connection &nic, unsigned limit)
	{
		unsigned cnt = 0;
		while (nic.tx()->ready_to_submit() && cnt < limit) {
			try {
				Packet_descriptor tx_packet = nic.tx()->alloc_packet(PACKET_SIZE);
				nic.tx()->submit_packet(tx_packet);
				cnt++;
			}
			catch (Nic::Session::Tx::Source::Packet_alloc_failed) { break; }
		}
		return cnt;
	}

	/*
	 * \return number of received acknowledgements
	 */
	static unsigned _collect_acknowledgements(Nic::Connection &nic)
	{
		unsigned cnt = 0;
		while (nic.tx()->ack_avail()) {
			Packet_descriptor acked_packet = nic.tx()->get_acked_packet();
			nic.tx()->release_packet(acked_packet);
			cnt++;
		}
		return cnt;
	}

	static unsigned _receive_all_incoming_packets(Nic::Connection &nic)
	{
		unsigned cnt = 0;
		while (nic.rx()->packet_avail() && nic.rx()->ready_to_ack()) {
			Packet_descriptor rx_packet = nic.rx()->get_packet();
			nic.rx()->acknowledge_packet(rx_packet);
			cnt++;
		}
		return cnt;
	}

	void _check_for_success()
	{
		unsigned const n = _num_packets;
		if (_acked_cnt == n && _tx_cnt == n && _rx_cnt == n)
			success();
	}

	void handle_nic() override
	{
		unsigned const max_outstanding_requests = Nic::Session::QUEUE_SIZE - 1;
		unsigned const outstanding_requests     = _tx_cnt - _rx_cnt;

		unsigned const tx_limit = min(_num_packets - _tx_cnt,
		                              max_outstanding_requests - outstanding_requests);

		unsigned const num_tx   = _send_packets(nic(), tx_limit);
		unsigned const num_acks = _collect_acknowledgements(nic());
		unsigned const num_rx   = _receive_all_incoming_packets(nic());

		_tx_cnt    += num_tx;
		_rx_cnt    += num_rx;
		_acked_cnt += num_acks;

		log("acked ", num_acks, " packets, "
		    "received ", num_rx, " packets "
		    "-> tx: ", _tx_cnt, ", acked: ", _acked_cnt, ", rx: ", _rx_cnt);

		_check_for_success();
	}

	Batch(Env &env, Signal_context_capability success_sigh, unsigned num_packets)
	:
		Base(env, "batch", success_sigh), _num_packets(num_packets)
	{
		handle_nic();
	}
};


struct Test::Main
{
	Env &_env;

	Constructible<Roundtrip> _roundtrip { };
	Constructible<Batch>     _batch     { };

	Signal_handler<Main> _test_completed_handler {
		_env.ep(), *this, &Main::_handle_test_completed };

	void _handle_test_completed()
	{
		if (_roundtrip.constructed()) {
			_roundtrip.destruct();

			enum { NUM_PACKETS = 1000 };
			_batch.construct(_env, _test_completed_handler, NUM_PACKETS);
			return;
		}

		if (_batch.constructed()) {
			_batch.destruct();
			log("--- finished NIC loop-back test ---");
			_env.parent().exit(0);
		}
	}

	Main(Env &env) : _env(env)
	{
		log("--- NIC loop-back test ---");

		_roundtrip.construct(_env, _test_completed_handler, "abcdefghijklmn");
	}
};


void Component::construct(Genode::Env &env) { static Test::Main main(env); }

