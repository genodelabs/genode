/*
 * \brief  Uplink session client role of the driver
 * \author Martin Stein
 * \date   2020-12-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local include */
#include <uplink_client.h>

#include <lx_emul.h>

extern "C" {
#include <lxc.h>
};


Genode::Uplink_client::Transmit_result
Genode::Uplink_client::_drv_transmit_pkt(const char *conn_rx_pkt_base,
                                         size_t      conn_rx_pkt_size)
{
	/*
	 * We must not be called from another task, just from the
	 * packet stream dispatcher.
	 */
	if (Lx::scheduler().active()) {
		warning("scheduler active");
		return Transmit_result::RETRY;
	}

	struct sk_buff *skb {
		lxc_alloc_skb(conn_rx_pkt_size +
		              HEAD_ROOM, HEAD_ROOM) };

	unsigned char *data = lxc_skb_put(skb, conn_rx_pkt_size);
	memcpy(data, conn_rx_pkt_base, conn_rx_pkt_size);

	_tx_data.ndev = _ndev;
	_tx_data.skb  = skb;

	_tx_task.unblock();
	Lx::scheduler().schedule();
	return Transmit_result::ACCEPTED;
}


Genode::Uplink_client::Uplink_client(Env                 &env,
                                     Allocator           &alloc,
                                     Session_label const &label)
:
	Linux_network_session_base { label },
	Uplink_client_base         { env, alloc,
	                             Net::Mac_address(_ndev->dev_addr) }
{
	_drv_handle_link_state(_read_link_state_from_ndev());
}


void Genode::Uplink_client::link_state(bool state)
{
	_drv_handle_link_state(state);
}


void Genode::Uplink_client::receive(struct sk_buff *skb)
{
	Skb skb_helpr { skb_helper(skb) };
	_drv_rx_handle_pkt(
		skb_helpr.packet_size + skb_helpr.frag_size,
		[&] (void   *conn_tx_pkt_base,
		     size_t &)
	{
		memcpy(
			conn_tx_pkt_base,
			skb_helpr.packet,
			skb_helpr.packet_size);

		if (skb_helpr.frag_size) {

			memcpy(
				(char *)conn_tx_pkt_base + skb_helpr.packet_size,
				skb_helpr.frag,
				skb_helpr.frag_size);
		}
		return Write_result::WRITE_SUCCEEDED;
	});
}
