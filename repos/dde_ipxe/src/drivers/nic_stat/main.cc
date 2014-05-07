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

/* Genode */
#include <base/env.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <cap_session/connection.h>
#include <nic/component.h>

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

			static void dde_rx_handler(unsigned if_index,
			                           const char *packet,
			                           unsigned packet_len)
			{
				instance->rx_handler_stat(packet, packet_len);
			}

		private:

			Nic::Mac_address      _mac_addr;
			Nic::Rx_buffer_alloc &_alloc;

			Timer::Connection _timer;
			Nic::Measurement _stat;

		public:

			Driver(Nic::Rx_buffer_alloc &alloc)
			: _alloc(alloc), _stat(_timer)
			{
				PINF("--- init iPXE NIC");
				int cnt = dde_ipxe_nic_init();
				PINF("    number of devices: %d", cnt);

				PINF("--- init rx_callbacks");
				dde_ipxe_nic_register_rx_callback(dde_rx_handler);

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


			/***************************
			 ** Nic::Driver interface **
			 ***************************/

			Nic::Mac_address mac_address() { return _mac_addr; }

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

	class Driver_factory : public Nic::Driver_factory
	{
		Nic::Driver *create(Nic::Rx_buffer_alloc &alloc)
		{
			Driver::instance = new (Genode::env()->heap()) Ipxe::Driver(alloc);
			return Driver::instance;
		}

		void destroy(Nic::Driver *)
		{
			Genode::destroy(Genode::env()->heap(), Driver::instance);
			Driver::instance = 0;
		}
	};

} /* namespace Ipxe */


Ipxe::Driver * Ipxe::Driver::instance = 0;


int main(int, char **)
{
	using namespace Genode;

	printf("--- iPXE NIC driver started ---\n");

	/**
	 * Factory used by 'Nic::Root' at session creation/destruction time
	 */
	static Ipxe::Driver_factory driver_factory;

	enum { STACK_SIZE = 2*sizeof(addr_t)*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "nic_ep");

	static Nic::Root nic_root(&ep, env()->heap(), driver_factory);
	env()->parent()->announce(ep.manage(&nic_root));

/*
    Genode::size_t           tx_buf_size = 64*1024;
    Genode::size_t           rx_buf_size = 64*1024;
	session("ram_quota=%zd, tx_buf_size=%zd, rx_buf_size=%zd",
	        6*4096 + tx_buf_size + rx_buf_size,
	        tx_buf_size, rx_buf_size))
*/
	nic_root.session("ram_quota=155648, tx_buf_size=65536, rx_buf_size=65536",
	                 Affinity());

	sleep_forever();
	return 0;
}

