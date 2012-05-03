/*
 * \brief  Genode C API framebuffer functions of the L4Linux support library
 * \author Stefan Kalkowski
 * \date   2009-06-08
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/allocator_avl.h>
#include <base/env.h>
#include <base/exception.h>
#include <base/printf.h>
#include <util/misc_math.h>
#include <util/string.h>
#include <nic_session/connection.h>

#include <vcpu.h>
#include <linux.h>

namespace Fiasco {
#include <genode/net.h>
#include <l4/sys/irq.h>
#include <l4/sys/kdebug.h>
}

static Nic::Connection *nic() {
	static Nic::Connection *n = 0;
	static bool initialized = false;

	if (!initialized) {
		try {
			static Genode::Allocator_avl tx_block_alloc(Genode::env()->heap());
			static Nic::Connection nic(&tx_block_alloc);
			n = &nic;
		} catch(...) { }
		initialized = true;
	}
	return n;
}

namespace {

	class Packet_pool
	{
		private:

			class Entry
			{
				public:

					Packet_descriptor packet;
					void             *addr;

					Entry() : addr(0) {}
			};


			enum { MAX_ENTRIES = 100 };

			Entry _entries[MAX_ENTRIES];

		public:

			class Pool_full : Genode::Exception {};


			void add(Packet_descriptor p, void* addr)
			{
				for (unsigned i=0; i < MAX_ENTRIES; i++) {
					if (!_entries[i].addr) {
						_entries[i].addr   = addr;
						_entries[i].packet = p;
						return;
					}
				}
				throw Pool_full();
			}

			void* get(Packet_descriptor _packet)
			{
				for (unsigned i=0; i < MAX_ENTRIES; i++)
					if (nic()->tx()->packet_content(_packet)
						== nic()->tx()->packet_content(_entries[i].packet)) {
						void *ret = _entries[i].addr;
						_entries[i].addr = 0;
						return ret;
					}
				return 0;
			}
	};


	static Packet_pool *packet_pool()
	{
		static Packet_pool pool;
		return &pool;
	}


	class Signal_thread : public Genode::Thread<8192>
	{
		private:

			Fiasco::l4_cap_idx_t _cap;

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

				while (true) {
					receiver.wait_for_signal();
					if (l4_error(l4_irq_trigger(_cap)) != -1)
						PWRN("IRQ net trigger failed\n");
				}
			}

		public:

			Signal_thread(Fiasco::l4_cap_idx_t cap)
			: Genode::Thread<8192>("net-signal-thread"), _cap(cap) { start(); }
	};
}

using namespace Fiasco;

extern "C" {

	static FASTCALL void (*receive_packet)(void*, void*, unsigned long) = 0;
	static void *net_device                                    = 0;


	void genode_net_start(void *dev, FASTCALL void (*func)(void*, void*, unsigned long))
	{
		receive_packet = func;
		net_device     = dev;
	}


	l4_cap_idx_t genode_net_irq_cap()
	{
		static Genode::Native_capability cap = L4lx::vcpu_connection()->alloc_irq();
		static Signal_thread th(cap.dst());
		return cap.dst();
	}


	void genode_net_stop()
	{
		net_device     = 0;
		receive_packet = 0;
	}


	void genode_net_mac(void* mac, unsigned long size)
	{
		using namespace Genode;

		Nic::Mac_address m = nic()->mac_address();
		memcpy(mac, &m.addr, min(sizeof(m.addr), (size_t)size));
	}


	int genode_net_tx(void* addr, unsigned long len, void *skb)
	{
		try {
			Packet_descriptor packet = nic()->tx()->alloc_packet(len);
			void* content            = nic()->tx()->packet_content(packet);
			packet_pool()->add(packet, skb);
			Genode::memcpy(content, addr, len);
			nic()->tx()->submit_packet(packet);
			return 0;
		} catch(Packet_pool::Pool_full) {
			PWRN("skb_buff/packet pool full!");
		} catch(...) {
			PWRN("Send failed!");
		}
		return 1;
	}


	int genode_net_tx_ack_avail() {
		return nic()->tx()->ack_avail(); }


	void* genode_net_tx_ack()
	{
		Packet_descriptor packet = nic()->tx()->get_acked_packet();
		void *skb = packet_pool()->get(packet);
		nic()->tx()->release_packet(packet);
		return skb;
	}


	void genode_net_rx_receive()
	{
		if (nic()) {
			while(nic()->rx()->packet_avail()) {
				Packet_descriptor p = nic()->rx()->get_packet();
				if (receive_packet && net_device)
					receive_packet(net_device, nic()->rx()->packet_content(p), p.size());
				nic()->rx()->acknowledge_packet(p);
			}
		}
	}


	int genode_net_ready()
	{
		return nic() ? 1 : 0;
	}
}
