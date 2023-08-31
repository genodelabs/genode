/*
 * \brief  Linux emulation code
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Emery Hemingway
 * \date   2013-08-28
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

/* format-string includes */
#include <format/snprintf.h>

/* local includes */
#include <lx.h>
#include <nic.h>

bool ic_link_state = false;


class Nic_client
{
	private:

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		Nic::Packet_allocator _tx_block_alloc;
		Nic::Connection       _nic;

		Genode::Io_signal_handler<Nic_client> _sink_ack;
		Genode::Io_signal_handler<Nic_client> _sink_submit;
		Genode::Io_signal_handler<Nic_client> _source_ack;
		Genode::Io_signal_handler<Nic_client> _link_state_change;

		void (*_tick)();

		void _link_state()
		{
			bool const link_state = _nic.link_state();
			ic_link_state = link_state;

			if (link_state == false || lxip_do_dhcp() == false)
				return;

			Lx::timer_update_jiffies();

			/* reconnect dhcp client */
			lxip_configure_dhcp();
		}

		/**
		 * submit queue not empty anymore
		 */
		void _packet_avail()
		{
			Lx::timer_update_jiffies();

			/* process a batch of only MAX_PACKETS in one run */
			enum { MAX_PACKETS = 20 };

			int count = 0;
			while (_nic.rx()->packet_avail() &&
			       _nic.rx()->ready_to_ack() &&
			       count++ < MAX_PACKETS)
			{
				Nic::Packet_descriptor p = _nic.rx()->get_packet();
				try { net_driver_rx(_nic.rx()->packet_content(p), p.size()); }
				catch (Genode::Packet_descriptor::Invalid_packet) {
					Genode::error("received invalid Nic packet"); }
				_nic.rx()->acknowledge_packet(p);
			}

			/* schedule next batch if there are still packets available */
			if (_nic.rx()->packet_avail())
				Genode::Signal_transmitter(_sink_submit).submit();

			/* tick the higher layer of the component */
			_tick();
		}

		/**
		 * acknoledgement queue not full anymore
		 */
		void _ready_to_ack()
		{
			_packet_avail();
		}

		/**
		 * acknoledgement queue not empty anymore
		 */
		void _ack_avail()
		{
			while (_nic.tx()->ack_avail()) {
				Nic::Packet_descriptor p = _nic.tx()->get_acked_packet();
				_nic.tx()->release_packet(p);
			}
		}


	public:

		Nic_client(Genode::Env &env,
		           Genode::Allocator &alloc,
		           void (*ticker)())
		:
			_tx_block_alloc(&alloc),
			_nic(env, &_tx_block_alloc, BUF_SIZE, BUF_SIZE),
			_sink_ack(env.ep(), *this, &Nic_client::_packet_avail),
			_sink_submit(env.ep(), *this, &Nic_client::_ready_to_ack),
			_source_ack(env.ep(), *this, &Nic_client::_ack_avail),
			_link_state_change(env.ep(), *this, &Nic_client::_link_state),
			_tick(ticker)
		{
			ic_link_state = _nic.link_state();

			_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
			_nic.rx_channel()->sigh_packet_avail(_sink_submit);
			_nic.tx_channel()->sigh_ack_avail(_source_ack);
			_nic.link_state_sigh(_link_state_change);
			/* ready_to_submit not handled */
		}

		Nic::Connection *nic() { return &_nic; }
};


static Nic_client *_nic_client;


void Lx::nic_client_init(Genode::Env &env,
	                       Genode::Allocator &alloc,
	                       void (*ticker)())
{
	static Nic_client _inst(env, alloc, ticker);
	_nic_client = &_inst;
}


/**
 * Call by back-end driver while initializing
 */
void net_mac(void* mac, unsigned long size)
{
	enum { MAC_LEN = 17, ETH_ALEN = 6, };

	char str[MAC_LEN + 1];
	using namespace Genode;

	Nic::Mac_address m = _nic_client->nic()->mac_address();
	Genode::memcpy(mac, &m.addr, min(sizeof(m.addr), (size_t)size));

	unsigned char const *mac_addr = (unsigned char const*)m.addr;
	for (int i = 0; i < ETH_ALEN; i++) {
		Format::snprintf((char *)&str[i * 3], 3, "%02x", mac_addr[i]);
		if ((i * 3) < MAC_LEN)
		str[(i * 3) + 2] = ':';
	}
	str[MAC_LEN] = 0;

	Genode::log("Received mac: ", Cstring(str));
}


/**
 * Call by back-end driver when a packet should be sent
 */
int net_tx(void* addr, unsigned long len)
{
	try {
		Nic::Packet_descriptor packet = _nic_client->nic()->tx()->alloc_packet(len);
		void* content                 = _nic_client->nic()->tx()->packet_content(packet);

		Genode::memcpy((char *)content, addr, len);
		_nic_client->nic()->tx()->submit_packet(packet);

		return 0;
	/* Packet_alloc_failed */
	} catch(...) { return 1; }
}
