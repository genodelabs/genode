/*
 * \brief  A simple nic session client using the performance measurement
 *         library. Purpose is to measure the overhead of a nic_session client
 *         using a ethernet driver versus solely using th ethernet driver.
 * \author Alexander Boettcher
 * \date   2013-03-26
 */

/*
 * Copyright (C) 2013-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/log.h>
#include <base/stdint.h>
#include <base/sleep.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <timer_session/connection.h>

#include <nic/stat.h>
#include <net/ethernet.h>

enum {
	STACK_SIZE = 4096,
};

class Nic_worker : public Genode::Thread_deprecated<STACK_SIZE>
{
	private:

		Nic::Connection  *_nic;       /* nic-session */
		Net::Mac_address _mac;

		struct stat {
			Genode::uint64_t size;
			Genode::uint64_t count; 
		} _stat, _drop;

		Genode::uint16_t _ntoh(Genode::uint16_t value) {
			return ((value & 0xFFU) << 8) | ((value >> 8) & 0xFFU); }

	public:

		Nic_worker(Nic::Connection *nic)
		:
			Genode::Thread_deprecated<STACK_SIZE>("nic-worker"), _nic(nic)
		{
			using namespace Genode;

			memset(&_stat, 0, sizeof(_stat));
			memset(&_drop, 0, sizeof(_drop));

			memcpy(_mac.addr, nic->mac_address().addr, 6);
			log("MAC: ", _mac);
		}

		void entry()
		{
			using namespace Genode;

			Timer::Connection timer;

			log("ready to receive packets");

			Nic::Measurement stat(timer);
			stat.set_mac(_mac.addr);

			while(true)
			{
				Packet_descriptor rx_packet = _nic->rx()->get_packet();

				Net::Ethernet_frame *eth =
					new (_nic->rx()->packet_content(rx_packet)) Net::Ethernet_frame(rx_packet.size());
				stat.data(eth, rx_packet.size());

				_nic->rx()->acknowledge_packet(rx_packet);
			}
		}
};

static void net_init()
{
	using namespace Genode;

	/* Initialize nic-session */
	enum {
		PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
		BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
	};

	Nic::Packet_allocator *tx_block_alloc = new (env()->heap()) Nic::Packet_allocator(env()->heap());

	Nic::Connection *nic = 0;
	try {
		nic = new (env()->heap()) Nic::Connection(tx_block_alloc, BUF_SIZE, BUF_SIZE);
	} catch (Parent::Service_denied) {
		destroy(env()->heap(), tx_block_alloc);
		Genode::error("could not start Nic service");
		return;
	}

	/* Setup receiver thread */
	Nic_worker *worker = new (env()->heap()) Nic_worker(nic);
	worker->start();

}

int main(int, char **)
{
	Genode::log("--- NIC performance measurements ---");

	net_init();

	Genode::sleep_forever();
	return 0;
}
