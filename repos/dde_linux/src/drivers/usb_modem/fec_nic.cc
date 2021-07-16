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

#include <fec_nic.h>
#include <legacy/lx_kit/scheduler.h>
#include <lx_emul.h>


void Fec_nic::_run_tx_task(void * args)
{
	Tx_data *data = static_cast<Tx_data*>(args);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		net_device *     ndev = data->ndev;
		struct sk_buff * skb  = data->skb;

		ndev->netdev_ops->ndo_start_xmit(skb, ndev);
	}
}


void Fec_nic::_run_rx_task(void * args)
{
	Rx_data *data = static_cast<Rx_data*>(args);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		int ret = 0;
		struct napi_struct * n = data->napi;

		for (;;) {

			/* This NAPI_STATE_SCHED test is for avoiding a race
			 * with netpoll's poll_napi().  Only the entity which
			 * obtains the lock and sees NAPI_STATE_SCHED set will
			 * actually make the ->poll() call.  Therefore we avoid
			 * accidentally calling ->poll() when NAPI is not scheduled.
			 */
			if (!test_bit(NAPI_STATE_SCHED, &n->state)) break;

			int weight = n->weight;
			int work   = n->poll(n, weight);
			ret       += work;

			if (work < weight) break;

			Genode::warning("Too much incoming traffic, we should "
			                "schedule RX more intelligent");
		}
	}
}


Fec_nic::Fec_nic(Genode::Session_label const &label)
:
	_ndev { _register_fec_nic(*this, label) }
{
	
	if (!_ndev) {

		throw Genode::Service_denied();
	}
}


bool Fec_nic::_read_link_state_from_ndev() const
{
	return !(_ndev->state & (1UL << __LINK_STATE_NOCARRIER));
}


void Fec_nic::unblock_rx_task(napi_struct * n)
{
	_rx_data.napi = n;
	_rx_task.unblock();
}
