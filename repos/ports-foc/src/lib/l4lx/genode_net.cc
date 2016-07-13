/*
 * \brief  Genode C API framebuffer functions of the L4Linux support library
 * \author Stefan Kalkowski
 * \date   2009-06-08
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/exception.h>
#include <base/log.h>
#include <util/misc_math.h>
#include <util/string.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <timer_session/connection.h>
#include <foc/capability_space.h>

#include <vcpu.h>
#include <linux.h>

namespace Fiasco {
#include <genode/net.h>
#include <l4/sys/irq.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/ktrace.h>
}

#define TX_BENCH 0
#define RX_BENCH 0


/**
 * Debugging/Tracing
 */
#if TX_BENCH | RX_BENCH
struct Counter : public Genode::Thread_deprecated<8192>
{
	int             cnt;
	Genode::size_t size;

	void entry()
	{
		Timer::Connection _timer;
		int interval = 5;
		while(1) {
			_timer.msleep(interval * 1000);
			Genode::log("LX Packets ", cnt/interval, "/s "
			            "bytes/s: ", size / interval);
			cnt = 0;
			size = 0;
		}
	}

	void inc(Genode::size_t s) { cnt++; size += s; }

	Counter() : Thread_deprecated("net-counter"), cnt(0), size(0)  { start(); }
};
#else
struct Counter { inline void inc(Genode::size_t s) { } };
#endif


static Nic::Connection *nic() {

	enum {
		PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
		BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
	};

	static Nic::Connection *n = 0;
	static bool initialized   = false;

	if (initialized)
		return n;

	try {
		Linux::Irq_guard guard;
		static Nic::Packet_allocator tx_block_alloc(Genode::env()->heap());
		static Nic::Connection nic(&tx_block_alloc, BUF_SIZE, BUF_SIZE);
		n = &nic;
	} catch(...) { }
	initialized = true;

	return n;
}


namespace {

	class Signal_thread : public Genode::Thread_deprecated<8192>
	{
		private:

			Fiasco::l4_cap_idx_t _cap;
			Genode::Lock        *_sync;

		protected:

			void entry()
			{
				using namespace Fiasco;
				using namespace Genode;

				Signal_receiver           receiver;
				Signal_context            rx;
				Signal_context_capability cap(receiver.manage(&rx));
				nic()->rx_channel()->sigh_ready_to_ack(cap);
				nic()->rx_channel()->sigh_packet_avail(cap);

				_sync->unlock();

				while (true) {
					receiver.wait_for_signal();

					if (l4_error(l4_irq_trigger(_cap)) != -1)
						warning("IRQ net trigger failed");
				}
			}

		public:

			Signal_thread(Fiasco::l4_cap_idx_t cap, Genode::Lock *sync)
			: Genode::Thread_deprecated<8192>("net-signal-thread"), _cap(cap), _sync(sync) {
				start(); }
	};
}

using namespace Fiasco;

extern "C" {

	static FASTCALL void (*receive_packet)(void*, void*, unsigned long) = 0;
	static void *net_device                                             = 0;


	void genode_net_start(void *dev, FASTCALL void (*func)(void*, void*, unsigned long))
	{
		receive_packet = func;
		net_device     = dev;
	}


	l4_cap_idx_t genode_net_irq_cap()
	{
		Linux::Irq_guard guard;
		Genode::Foc_native_cpu_client
			native_cpu(L4lx::cpu_connection()->native_cpu());
		static Genode::Native_capability cap = native_cpu.alloc_irq();
		static Genode::Lock lock(Genode::Lock::LOCKED);
		static Fiasco::l4_cap_idx_t const kcap = Genode::Capability_space::kcap(cap);
		static Signal_thread th(kcap, &lock);
		lock.lock();
		return kcap;
	}


	void genode_net_stop()
	{
		net_device     = 0;
		receive_packet = 0;
	}


	void genode_net_mac(void* mac, unsigned long size)
	{
		Linux::Irq_guard guard;
		using namespace Genode;

		Nic::Mac_address m = nic()->mac_address();
		memcpy(mac, &m.addr, min(sizeof(m.addr), (size_t)size));
	}


	int genode_net_tx(void* addr, unsigned long len)
	{
		Linux::Irq_guard guard;
		static Counter counter;

		try {
			Nic::Packet_descriptor packet = nic()->tx()->alloc_packet(len);
			void* content                 = nic()->tx()->packet_content(packet);

			Genode::memcpy((char *)content, addr, len);
			nic()->tx()->submit_packet(packet);

			counter.inc(len);

			return 0;
		/* 'Packet_alloc_failed' */
		} catch(...) {
			return 1;
		}
	}


	int genode_net_tx_ack_avail() {
		return nic()->tx()->ack_avail(); }


	void genode_net_tx_ack()
	{
		Linux::Irq_guard guard;

		Nic::Packet_descriptor packet = nic()->tx()->get_acked_packet();
		nic()->tx()->release_packet(packet);
	}


	void genode_net_rx_receive()
	{
		Linux::Irq_guard guard;
		static Counter counter;

		if (nic()) {
			while(nic()->rx()->packet_avail()) {
				Nic::Packet_descriptor p = nic()->rx()->get_packet();

				if (receive_packet && net_device)
					receive_packet(net_device, nic()->rx()->packet_content(p), p.size());
				
				counter.inc(p.size());
				nic()->rx()->acknowledge_packet(p);
			}
		}
	}


	int genode_net_ready()
	{
		return nic() ? 1 : 0;
	}


	void *genode_net_memcpy(void *dst, void const *src, unsigned long size) {
		return Genode::memcpy(dst, src, size); }
}
