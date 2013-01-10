/*
 * \brief  Genode C API framebuffer functions of the OKLinux support library
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
#include <base/printf.h>
#include <util/misc_math.h>
#include <util/string.h>
#include <nic_session/connection.h>

extern "C" {
#include <genode/config.h>
}

static bool avail = false;

static Nic::Connection *nic() {
	try {
		static Genode::Allocator_avl tx_block_alloc(Genode::env()->heap());
		static Nic::Connection nic(&tx_block_alloc);
		avail = true;
		return &nic;
	} catch(...) { }
	return 0;
}


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


extern "C" {

#include <genode/net.h>

	static void (*receive_packet)(void*, void*, unsigned long) = 0;
	static void *net_device                                    = 0;


	void genode_net_start(void *dev, void (*func)(void*, void*, unsigned long))
	{
		receive_packet = func;
		net_device     = dev;
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
		if (avail) {
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
		return genode_config_nic() && nic();
	}
}
