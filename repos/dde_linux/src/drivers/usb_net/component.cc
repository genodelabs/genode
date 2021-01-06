/*
 * \brief  Freescale ethernet driver component
 * \author Stefan Kalkowski
 * \date   2017-11-01
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <component.h>
#include <lx_emul.h>

extern "C" {
#include <lxc.h>
};


bool Session_component::_send()
{
	using namespace Genode;

	/*
	 * We must not be called from another task, just from the
	 * packet stream dispatcher.
	 */
	if (Lx::scheduler().active()) {
		warning("scheduler active");
		return false;
	}

	if (!_tx.sink()->ready_to_ack()) { return false; }
	if (!_tx.sink()->packet_avail()) { return false; }

	Packet_descriptor packet = _tx.sink()->get_packet();
	if (!packet.size() || !_tx.sink()->packet_valid(packet)) {
		warning("invalid tx packet");
		return true;
	}

	struct sk_buff *skb = lxc_alloc_skb(packet.size() + HEAD_ROOM, HEAD_ROOM);

	unsigned char *data = lxc_skb_put(skb, packet.size());
	Genode::memcpy(data, _tx.sink()->packet_content(packet), packet.size());

	_tx_data.ndev = _ndev;
	_tx_data.skb  = skb;

	_tx_task.unblock();
	Lx::scheduler().schedule();

	_tx.sink()->acknowledge_packet(packet);

	return true;
}


void Session_component::_handle_rx()
{
	while (_rx.source()->ack_avail())
		_rx.source()->release_packet(_rx.source()->get_acked_packet());
}


void Session_component::_handle_packet_stream()
{
	_handle_rx();

	while (_send()) continue;
}


Nic::Mac_address Session_component::mac_address()
{
	return _ndev ? Nic::Mac_address(_ndev->dev_addr) : Nic::Mac_address();
}


void Session_component::receive(struct sk_buff *skb)
{
	_handle_rx();

	if (!_rx.source()->ready_to_submit()) {
		Genode::warning("not ready to receive packet");
			return;
	}

	Skb s = skb_helper(skb);

	try {
		Nic::Packet_descriptor p = _rx.source()->alloc_packet(s.packet_size + s.frag_size);
		void *buffer = _rx.source()->packet_content(p);
		memcpy(buffer, s.packet, s.packet_size);

		if (s.frag_size)
			memcpy((char *)buffer + s.packet_size, s.frag, s.frag_size);

		_rx.source()->submit_packet(p);
	} catch (...) {
		Genode::warning("failed to process received packet");
	}
}


void Session_component::link_state(bool link)
{
	if (link == _has_link) return;

	_has_link = link;
	_link_state_changed();
}


Session_component::Session_component(Genode::size_t        const  tx_buf_size,
                                     Genode::size_t        const  rx_buf_size,
                                     Genode::Allocator           &rx_block_md_alloc,
                                     Genode::Env                 &env,
                                     Genode::Session_label const &label)
:
	Nic::Session_component     { tx_buf_size, rx_buf_size, Genode::CACHED,
	                             rx_block_md_alloc, env },
	Linux_network_session_base { label },
	_has_link                  { _read_link_state_from_ndev() }
{ }
