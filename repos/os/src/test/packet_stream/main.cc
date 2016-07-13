/*
 * \brief  Test for the packet-streaming interface
 * \author Norman Feske
 * \date   2009-11-11
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/env.h>
#include <base/thread.h>
#include <base/allocator_avl.h>
#include <timer_session/connection.h>
#include <os/packet_stream.h>


/**
 * Specialized policy, using a small queue sizes
 *
 * Note that the ack queue size is smaller than the submit queue
 * size.
 */
typedef Genode::Packet_stream_policy<Genode::Packet_descriptor, 8, 4, char>
        Test_packet_stream_policy;


enum { STACK_SIZE = 4096 };


void Genode::Packet_stream_base::_debug_print_buffers()
{
	Genode::log("_ds_local_base       = ", _ds_local_base);
	Genode::log("_submit_queue_offset = ", Genode::Hex(_submit_queue_offset));
	Genode::log("_ack_queue_offset    = ", Genode::Hex(_ack_queue_offset));
	Genode::log("_bulk_buffer_offset  = ", Genode::Hex(_bulk_buffer_offset));
	Genode::log("_bulk_buffer_size    = ", Genode::Hex(_bulk_buffer_size));
}


/**
 * Thread generating packets
 */
class Source : private Genode::Thread_deprecated<STACK_SIZE>,
               private Genode::Allocator_avl,
               public  Genode::Packet_stream_source<Test_packet_stream_policy>
{
	private:

		enum Operation { OP_NONE, OP_GENERATE, OP_ACKNOWLEDGE };

		Operation    _operation;  /* current mode of operation */
		Genode::Lock _lock;       /* lock used as barrier in the thread loop */
		unsigned     _cnt;        /* number of packets to produce */

		void _generate_packets(unsigned cnt)
		{
			for (unsigned i = 0; i < cnt; i++) {
				try {
					enum { PACKET_SIZE = 1024 };
					Packet_descriptor packet = alloc_packet(PACKET_SIZE);

					char *content = packet_content(packet);
					if (!content) {
						Genode::warning("source: invalid packet");
					}
					Genode::log("Source: allocated packet (offset=0x%lx, size=0x%zd\n",
					               packet.offset(), packet.size());

					for (unsigned i = 0; i < packet.size(); i++)
						content[i] = i;

					bool is_blocking = !ready_to_submit();
					if (is_blocking)
						Genode::log("Source: submit queue is full, going to block");

					submit_packet(packet);

					if (is_blocking)
						Genode::log("Source: returned from submit_packet function");

				} catch (Packet_stream_source<>::Packet_alloc_failed) {
					Genode::error("Source: Packet allocation failed");
				}
			}
		}

		void _acknowledge_packets(unsigned cnt)
		{
			for (unsigned i = 0; i < cnt; i++) {
				if (!ack_avail())
					Genode::log("Source: acknowledgement queue is empty, going to block");

				Packet_descriptor packet = get_acked_packet();
				char *content = packet_content(packet);
				if (!content) {
					Genode::warning("source: invalid packet");

				} else {

					/* validate packet content */
					for (unsigned i = 0; i < packet.size(); i++) {
						if (content[i] != (char)i) {
							Genode::error("source: packet content is corrupted");
							break;
						}
					}
				}

				Genode::log("Source: release packet ("
				            "offset=", Genode::Hex(packet.offset()), ", "
				            "size=",   packet.size(), ")");
				release_packet(packet);
			}
		}

		void entry()
		{
			for (;;) {

				/* wait for a new 'generate' or 'acknowledge' call */
				_lock.lock();

				if (_operation == OP_GENERATE)
					_generate_packets(_cnt);

				if (_operation == OP_ACKNOWLEDGE)
					_acknowledge_packets(_cnt);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Source(Genode::Dataspace_capability ds_cap)
		:
			/* init bulk buffer allocator, storing its meta data on the heap */
			Thread_deprecated("source"),
			Genode::Allocator_avl(Genode::env()->heap()),
			Packet_stream_source<Test_packet_stream_policy>(this, ds_cap),
			_operation(OP_NONE),
			_lock(Genode::Lock::LOCKED),
			_cnt(0)
		{
			Genode::log("Source: packet stream buffers:");
			debug_print_buffers();
			start();
		}

		void generate(unsigned cnt)
		{
			_cnt = cnt;
			_operation = OP_GENERATE;
			_lock.unlock();
		}

		void acknowledge(unsigned cnt)
		{
			_cnt = cnt;
			_operation = OP_ACKNOWLEDGE;
			_lock.unlock();
		}
};


class Sink : private Genode::Thread_deprecated<STACK_SIZE>,
             public  Genode::Packet_stream_sink<Test_packet_stream_policy>
{
	private:

		enum Operation { OP_NONE, OP_PROCESS };

		Operation    _operation;  /* current mode of operation */
		Genode::Lock _lock;       /* lock used as barrier in the thread loop */
		unsigned     _cnt;        /* number of packets to produce */

		void _process_packets(unsigned cnt)
		{
			for (unsigned i = 0; i < cnt; i++) {

				if (!packet_avail())
					Genode::log("Sink: no packet available, going to block");

				Packet_descriptor packet = get_packet();

				char *content = packet_content(packet);
				if (!content)
					Genode::warning("invalid packet");

				Genode::log("Sink: got packet ("
				            "offset=", Genode::Hex(packet.offset()), ", "
				            "size=",   packet.size(), ")");

				if (!ready_to_ack())
					Genode::log("Sink: ack queue is full, going to block");

				acknowledge_packet(packet);
			}
		}

		void entry()
		{
			for (;;) {

				/* wait for a new 'process' call */
				_lock.lock();

				if (_operation == OP_PROCESS)
					_process_packets(_cnt);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Sink(Genode::Dataspace_capability ds_cap)
		:
			Thread_deprecated("sink"),
			Packet_stream_sink<Test_packet_stream_policy>(ds_cap),
			_operation(OP_NONE),
			_lock(Genode::Lock::LOCKED),
			_cnt(0)
		{
			Genode::log("Sink: packet stream buffers:");
			debug_print_buffers();
			start();
		}

		void process(unsigned cnt)
		{
			_cnt = cnt;
			_operation = OP_PROCESS;
			_lock.unlock();
		}
};


void test_1_good_case(Timer::Session *timer, Source *source, Sink *sink,
                      unsigned batch_size, unsigned rounds)
{
	for (unsigned i = 0; i < rounds; i++) {

		enum { DELAY = 200 };

		Genode::log("- round ", i, " -");

		Genode::log("generate ", batch_size, " packets, "
		            "fitting in bulk buffer and submit queue");
		source->generate(batch_size);

		timer->msleep(DELAY);

		Genode::log("process ", batch_size, " packets");
		sink->process(batch_size);

		timer->msleep(DELAY);

		Genode::log("acknowledge ",  batch_size, " packets");
		source->acknowledge(batch_size);

		timer->msleep(DELAY);
	}
}


void test_2_flood_submit(Timer::Session *timer, Source *source, Sink *sink)
{
	enum { PACKETS = 9 }; /* more than the number of submit queue entries */
	enum { DELAY = 200 };

	source->generate(PACKETS);
	timer->msleep(DELAY);

	Genode::log("- source should block, process 3 packets, source should wake up -");

	sink->process(1);
	timer->msleep(5*DELAY);
	sink->process(2);

	Genode::log("- let source acknowledge 3 packets -");
	source->acknowledge(3);
	timer->msleep(DELAY);

	Genode::log("- process and acknowledge the remaining packets in batches 3 -\n");
	for (int i = 0; i < 2; i++) {
		sink->process(3);
		timer->msleep(DELAY);

		source->acknowledge(3);
		timer->msleep(DELAY);
	}
}


using namespace Genode;

int main(int, char **)
{
	log("--- packet stream test ---");

	Timer::Connection timer;

	enum { TRANSPORT_DS_SIZE = 16*1024 };
	Dataspace_capability ds_cap = env()->ram_session()->alloc(TRANSPORT_DS_SIZE);

	Source source(ds_cap);
	Sink   sink(ds_cap);

	/* wire data-flow signals beteen source and sink */
	source.register_sigh_packet_avail(sink.sigh_packet_avail());
	source.register_sigh_ready_to_ack(sink.sigh_ready_to_ack());
	sink.register_sigh_ready_to_submit(source.sigh_ready_to_submit());
	sink.register_sigh_ack_avail(source.sigh_ack_avail());

	timer.msleep(1000);

	log("\n-- test 1: good case, no queue pressure, no blocking  --");
	test_1_good_case(&timer, &source, &sink, 3, 5);

	log("\n-- test 2: flood submit queue, sender blocks, gets woken up  --");
	test_2_flood_submit(&timer, &source, &sink);

	log("waiting to settle down");
	timer.msleep(2*1000);

	log("--- end of packet stream test ---");
	return 0;
}
