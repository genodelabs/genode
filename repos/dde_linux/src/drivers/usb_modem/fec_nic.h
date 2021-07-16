/*
 * \brief  Virtual interface of a network session connected to the driver
 * \author Martin Stein
 * \date   2020-12-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SRC__DRIVERS__NIC__FEC__FEC_NIC_H_
#define _SRC__DRIVERS__NIC__FEC__FEC_NIC_H_

#include <legacy/lx_kit/scheduler.h>

/* forward declarations */
extern "C" {
	struct net_device;
	struct napi_struct;
	struct sk_buff;
}


class Fec_nic
{
	protected:

		enum { HEAD_ROOM = 8 };

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
		Tx_data      _tx_data;
		Rx_data      _rx_data;
		Lx::Task     _tx_task { _run_tx_task, &_tx_data, "tx_task",
		                        Lx::Task::PRIORITY_1, Lx::scheduler() };
		Lx::Task     _rx_task { _run_rx_task, &_rx_data, "rx_task",
		                        Lx::Task::PRIORITY_1, Lx::scheduler() };

		static void _run_tx_task(void * args);

		static void _run_rx_task(void * args);

		static net_device *_register_fec_nic(Fec_nic               &fec_nic,
		                                     Genode::Session_label  label);

		bool _read_link_state_from_ndev() const;

	public:

		Fec_nic(Genode::Session_label const &label);

		void unblock_rx_task(napi_struct * n);


		/***********************
		 ** Virtual interface **
		 ***********************/

		virtual void receive(struct sk_buff *skb) = 0;

		virtual void link_state(bool state) = 0;
};

#endif /* _SRC__DRIVERS__NIC__FEC__FEC_NIC_H_ */
