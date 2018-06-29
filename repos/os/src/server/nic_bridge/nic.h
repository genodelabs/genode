/*
 * \brief  Proxy-ARP NIC session handler
 * \author Stefan Kalkowski
 * \date   2010-08-18
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__NIC_BRIDGE__NIC_H_
#define _SRC__SERVER__NIC_BRIDGE__NIC_H_

#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

#include <packet_handler.h>

namespace Net { class Nic; }


class Net::Nic : public Net::Packet_handler
{
	private:

		enum {
			PACKET_SIZE = ::Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = ::Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		::Nic::Packet_allocator     _tx_block_alloc;
		::Nic::Connection           _nic;
		Mac_address _mac;

	public:

		Nic(Genode::Env&,
		    Genode::Heap&,
		    Vlan&,
		    bool                  const &verbose,
		    Genode::Session_label const &label);

		::Nic::Connection          *nic() { return &_nic; }
		Mac_address mac() { return _mac; }

		bool link_state() { return _nic.link_state(); }

		/******************************
		 ** Packet_handler interface **
		 ******************************/

		Packet_stream_sink< ::Nic::Session::Policy> * sink() override {
			return _nic.rx(); }

		Packet_stream_source< ::Nic::Session::Policy> * source() override {
			return _nic.tx(); }

		bool handle_arp(Ethernet_frame &eth,
		                Size_guard     &size_guard) override;

		bool handle_ip(Ethernet_frame &eth,
		               Size_guard     &size_guard) override;

		void finalize_packet(Ethernet_frame *, Genode::size_t) override { }
};

#endif /* _SRC__SERVER__NIC_BRIDGE__NIC_H_ */
