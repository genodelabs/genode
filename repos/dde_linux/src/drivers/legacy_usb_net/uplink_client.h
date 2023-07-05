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

#ifndef _SRC__DRIVERS__NIC__FEC__UPLINK_CLIENT_H_
#define _SRC__DRIVERS__NIC__FEC__UPLINK_CLIENT_H_

/* NIC driver includes */
#include <drivers/nic/uplink_client_base.h>

/* local include */
#include <linux_network_session_base.h>

namespace Genode {

	class Uplink_client;
}


class Genode::Uplink_client : public Linux_network_session_base,
                              public Uplink_client_base
{
	private:

		/************************
		 ** Uplink_client_base **
		 ************************/

		Transmit_result
		_drv_transmit_pkt(const char *conn_rx_pkt_base,
		                  size_t      conn_rx_pkt_size) override;

	public:

		Uplink_client(Env                 &env,
		              Allocator           &alloc,
		              Session_label const &label);


		/********************************
		 ** Linux_network_session_base **
		 ********************************/

		void link_state(bool state) override;

		void receive(struct sk_buff *skb) override;
};

#endif /* _SRC__DRIVERS__NIC__FEC__UPLINK_CLIENT_H_ */
