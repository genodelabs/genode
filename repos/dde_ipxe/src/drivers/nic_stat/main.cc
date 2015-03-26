/*
 * \brief  NIC driver based on iPXE for performance measurements solely
 * \author Christian Helmuth
 * \date   2011-11-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode include */
#include <base/env.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <cap_session/connection.h>
#include <nic/component.h>
#include <os/server.h>

#include <nic/stat.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

/* DDE */
extern "C" {
#include <dde_ipxe/nic.h>
}


namespace Ipxe {

	class Driver : public Nic::Driver
	{
		public:

			static Driver *instance;

		private:

			Nic::Mac_address          _mac_addr;
			Nic::Rx_buffer_alloc     &_alloc;
			Nic::Driver_notification &_notify;

			Timer::Connection _timer;
			Nic::Measurement  _stat;

			static void _rx_callback(unsigned    if_index,
			                         const char *packet,
			                         unsigned    packet_len)
			{
				instance->rx_handler_stat(packet, packet_len);
			}

			static void _link_callback() { instance->link_state_changed(); }

		public:

			Driver(Server::Entrypoint &ep, Nic::Rx_buffer_alloc &alloc,
			       Nic::Driver_notification &notify)
			: _alloc(alloc), _notify(notify), _stat(_timer)
			{
				PINF("--- init iPXE NIC");
				int cnt = dde_ipxe_nic_init(&ep);
				PINF("    number of devices: %d", cnt);

				PINF("--- init callbacks");
				dde_ipxe_nic_register_callbacks(_rx_callback, _link_callback);

				dde_ipxe_nic_get_mac_addr(1, _mac_addr.addr);
				PINF("--- get MAC address %02x:%02x:%02x:%02x:%02x:%02x",
				     _mac_addr.addr[0] & 0xff, _mac_addr.addr[1] & 0xff,
				     _mac_addr.addr[2] & 0xff, _mac_addr.addr[3] & 0xff,
				     _mac_addr.addr[4] & 0xff, _mac_addr.addr[5] & 0xff);

				_stat.set_mac(_mac_addr.addr);
			}

			void rx_handler_stat(const char *packet, unsigned packet_len)
			{
				Genode::addr_t test = reinterpret_cast<Genode::addr_t>(packet);
				void * buffer = reinterpret_cast<void *>(test);

				Net::Ethernet_frame *eth =
					new (buffer) Net::Ethernet_frame(packet_len);
				_stat.data(eth, packet_len);
			}

			void rx_handler(const char *packet, unsigned packet_len)
			{
				void *buffer = _alloc.alloc(packet_len);
				Genode::memcpy(buffer, packet, packet_len);

				_alloc.submit();
			}

			void link_state_changed() { _notify.link_state_changed(); }


			/***************************
			 ** Nic::Driver interface **
			 ***************************/

			Nic::Mac_address mac_address() { return _mac_addr; }

			bool link_state() override
			{
				return dde_ipxe_nic_link_state(1);
			}

			void tx(char const *packet, Genode::size_t size)
			{
				if (dde_ipxe_nic_tx(1, packet, size))
					PWRN("Sending packet failed!");
			}

			/******************************
			 ** Irq_activation interface **
			 ******************************/

			void handle_irq(int) { /* not used */ }
	};
} /* namespace Ipxe */


Ipxe::Driver * Ipxe::Driver::instance = 0;


struct Main
{
	Server::Entrypoint   &ep;
	Genode::Sliced_heap   sliced_heap;

	struct Factory : public Nic::Driver_factory
	{
		Server::Entrypoint &ep;

		Factory(Server::Entrypoint &ep) : ep(ep) { }

		Nic::Driver *create(Nic::Rx_buffer_alloc &alloc,
							Nic::Driver_notification &notify)
		{
			Ipxe::Driver::instance = new (Genode::env()->heap()) Ipxe::Driver(ep, alloc, notify);
			return Ipxe::Driver::instance;
		}

		void destroy(Nic::Driver *)
		{
			Genode::destroy(Genode::env()->heap(), Ipxe::Driver::instance);
			Ipxe::Driver::instance = 0;
		}
	} factory;

	Nic::Root root;

	Main(Server::Entrypoint &ep)
		:
			ep(ep),
			sliced_heap(Genode::env()->ram_session(), Genode::env()->rm_session()),
			factory(ep),
			root(&ep.rpc_ep(), &sliced_heap, factory)
	{
		PINF("--- iPXE NIC driver started ---\n");
		Genode::env()->parent()->announce(ep.manage(root));

		root.session("ram_quota=155648, tx_buf_size=65536, rx_buf_size=65536",
	                 Genode::Affinity());
	}
};


/************
 ** Server **
 ************/

namespace Server {
	char const *name()             { return "nic_drv_stat_ep";   }
	size_t      stack_size()       { return 2*1024*sizeof(long); }
	void construct(Entrypoint &ep) { static Main server(ep);     }
}
