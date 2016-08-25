/**
 * \brief  Linux emulation code
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2013-08-28
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

/* local includes */
#include <lx.h>
#include <nic.h>


class Nic_client
{
	private:

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		Nic::Packet_allocator _tx_block_alloc;
		Nic::Connection       _nic;

		Genode::Signal_dispatcher<Nic_client> _sink_ack;
		Genode::Signal_dispatcher<Nic_client> _sink_submit;
		Genode::Signal_dispatcher<Nic_client> _source_ack;
		Genode::Signal_dispatcher<Nic_client> _source_submit;

		/**
		 * submit queue not empty anymore
		 */
		void _packet_avail(unsigned)
		{
			enum { MAX_PACKETS = 20 };

			int count = 0;
			while (_nic.rx()->packet_avail() &&
			       _nic.rx()->ready_to_ack() &&
			       count++ < MAX_PACKETS)
			{
				Nic::Packet_descriptor p = _nic.rx()->get_packet();
				net_driver_rx(_nic.rx()->packet_content(p), p.size());

				_nic.rx()->acknowledge_packet(p);
			}

			if (_nic.rx()->packet_avail())
				Genode::Signal_transmitter(_sink_submit).submit();
		}

		/**
		 * acknoledgement queue not full anymore
		 */
		void _ready_to_ack(unsigned num)
		{
			_packet_avail(num);
		}

		/**
		 * acknoledgement queue not empty anymore
		 */
		void _ack_avail(unsigned)
		{
			while (_nic.tx()->ack_avail()) {
				Nic::Packet_descriptor p = _nic.tx()->get_acked_packet();
				_nic.tx()->release_packet(p);
			}
		}

		/**
		 * submit queue not full anymore
		 *
		 * TODO: by now, we just drop packets that cannot be transferred
		 *       to the other side, that's why we ignore this signal.
		 */
		void _ready_to_submit(unsigned) { }

	public:

		Nic_client(Genode::Signal_receiver &sig_rec)
		:
			_tx_block_alloc(Genode::env()->heap()),
			_nic(&_tx_block_alloc, BUF_SIZE, BUF_SIZE),
			_sink_ack(sig_rec, *this, &Nic_client::_ready_to_ack),
			_sink_submit(sig_rec, *this, &Nic_client::_packet_avail),
			_source_ack(sig_rec, *this, &Nic_client::_ack_avail),
			_source_submit(sig_rec, *this, &Nic_client::_ready_to_submit)
		{
			_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
			_nic.rx_channel()->sigh_packet_avail(_sink_submit);
			_nic.tx_channel()->sigh_ack_avail(_source_ack);
			_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
		}

		Nic::Connection *nic() { return &_nic; }
};


static Nic_client *_nic_client;


void Lx::nic_client_init(Genode::Signal_receiver &sig_rec)
{
	static Nic_client _inst(sig_rec);
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
		Genode::snprintf((char *)&str[i * 3], 3, "%02x", mac_addr[i]);
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
