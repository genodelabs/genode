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

#ifndef _SRC__DRIVERS__NIC__FEC__COMPONENT_H_
#define _SRC__DRIVERS__NIC__FEC__COMPONENT_H_

/* Genode includes */
#include <nic/root.h>

#include <lx_kit/scheduler.h>

extern "C" {
	struct net_device;
	struct napi_struct;
	struct sk_buff;
}

class Session_component : public Nic::Session_component
{
	private:

		struct Tx_data
		{
			net_device * ndev;
			sk_buff    * skb;
		};

		struct Rx_data
		{
			struct napi_struct * napi;
		};

		net_device * _ndev     = nullptr;
		bool         _has_link = false;
		Tx_data      _tx_data;
		Rx_data      _rx_data;

		static void _run_tx_task(void * args);
		static void _run_rx_task(void * args);
		static void _register_session_component(Session_component &);

		Lx::Task _tx_task { _run_tx_task, &_tx_data, "tx_task",
		                    Lx::Task::PRIORITY_1, Lx::scheduler() };
		Lx::Task _rx_task { _run_rx_task, &_rx_data, "rx_task",
		                    Lx::Task::PRIORITY_1, Lx::scheduler() };

		bool _send();
		void _handle_rx();
		void _handle_packet_stream() override;

	public:

		Session_component(Genode::size_t const tx_buf_size,
		                  Genode::size_t const rx_buf_size,
		                  Genode::Allocator &  rx_block_md_alloc,
		                  Genode::Env &        env);

		Nic::Mac_address mac_address() override;
		bool link_state() override { return _has_link; }
		void link_state(bool link);
		void receive(struct sk_buff *skb);
		void unblock_rx_task(napi_struct * n);
};

#endif /* _SRC__DRIVERS__NIC__FEC__COMPONENT_H_ */
