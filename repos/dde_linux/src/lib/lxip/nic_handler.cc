#include <base/printf.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>

#include <env.h>
#include <nic.h>
#include <packet_handler.h>

enum { 
	MAC_LEN = 17,
	ETH_ALEN = 6,
};

namespace Net {
	class Nic;
}


class Net::Nic : public Net::Packet_handler
{
	private:

		enum {
			PACKET_SIZE = ::Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = ::Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		::Nic::Packet_allocator _tx_block_alloc;
		::Nic::Connection       _nic;

	public:

		Nic()
		: _tx_block_alloc(Genode::env()->heap()),
		  _nic(&_tx_block_alloc, BUF_SIZE, BUF_SIZE)
		{
			_nic.rx_channel()->sigh_ready_to_ack(_sink_ack);
			_nic.rx_channel()->sigh_packet_avail(_sink_submit);
			_nic.tx_channel()->sigh_ack_avail(_source_ack);
			_nic.tx_channel()->sigh_ready_to_submit(_source_submit);
		}

		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink< ::Nic::Session::Policy> * sink() {
			return _nic.rx(); }

		Packet_stream_source< ::Nic::Session::Policy> * source() {
			return _nic.tx(); }

		static Nic *nic()
		{
			static Nic _net_nic;
			return &_net_nic;
		}

		static ::Nic::Connection *n()
		{
			return &nic()->_nic;
		}

};


Net::Packet_handler::Packet_handler()
: _sink_ack(*Net::Env::receiver(), *this, &Packet_handler::_ready_to_ack),
  _sink_submit(*Net::Env::receiver(), *this, &Packet_handler::_packet_avail),
  _source_ack(*Net::Env::receiver(), *this, &Packet_handler::_ack_avail),
  _source_submit(*Net::Env::receiver(), *this, &Packet_handler::_ready_to_submit)
{ }


void Net::Packet_handler::_ack_avail(unsigned)
{
	while (source()->ack_avail())
		source()->release_packet(source()->get_acked_packet());
}


void Net::Packet_handler::_ready_to_ack(unsigned num)
{
	_packet_avail(num);
}


void Net::Packet_handler::_packet_avail(unsigned)
{
	using namespace Net;
	enum { MAX_PACKETS = 20 };

	int count = 0;
	while(Nic::n()->rx()->packet_avail() &&
	      Nic::n()->rx()->ready_to_ack() &&
	      count++ < MAX_PACKETS)
	{
		Packet_descriptor p = Nic::n()->rx()->get_packet();
		net_driver_rx(Net::Nic::n()->rx()->packet_content(p), p.size());
		Nic::n()->rx()->acknowledge_packet(p);
	}

	if (Nic::n()->rx()->packet_avail())
		Genode::Signal_transmitter(_sink_submit).submit();
}


static void snprint_mac(unsigned char *buf, unsigned char *mac)
{
	for (int i = 0; i < ETH_ALEN; i++) {
		Genode::snprintf((char *)&buf[i * 3], 3, "%02x", mac[i]);
		if ((i * 3) < MAC_LEN)
		buf[(i * 3) + 2] = ':';
	}

	buf[MAC_LEN] = 0;
}


void net_mac(void* mac, unsigned long size)
{
	unsigned char str[MAC_LEN + 1];
	using namespace Genode;

	Nic::Mac_address m = Net::Nic::n()->mac_address();
	Genode::memcpy(mac, &m.addr, min(sizeof(m.addr), (size_t)size));

	snprint_mac(str, (unsigned char *)m.addr);
	PINF("Received mac: %s", str);
}


int net_tx(void* addr, unsigned long len)
{
	try {
		Net::Packet_descriptor packet = Net::Nic::n()->tx()->alloc_packet(len);
		void* content                 = Net::Nic::n()->tx()->packet_content(packet);

		Genode::memcpy((char *)content, addr, len);
		Net::Nic::n()->tx()->submit_packet(packet);

		return 0;
	/* 'Packet_alloc_failed' */
	} catch(...) {
		return 1;
	}
}
