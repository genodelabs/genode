/*
 * \brief  Generic base of a network session using a DDE Linux back end
 * \author Martin Stein
 * \date   2020-12-13
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux_network_session_base.h>
#include <legacy/lx_kit/scheduler.h>
#include <lx_emul.h>


void Linux_network_session_base::_run_tx_task(void * args)
{
	Tx_data *data = static_cast<Tx_data*>(args);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		net_device *     ndev = data->ndev;
		struct sk_buff * skb  = data->skb;

		ndev->netdev_ops->ndo_start_xmit(skb, ndev);
	}
}


void Linux_network_session_base::_run_rx_task(void * args)
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


Linux_network_session_base::
Linux_network_session_base(Genode::Session_label const &label)
:
	_ndev { _register_session(*this, label) }
{
	if (!_ndev) {
		error("failed to register session with label \"", label, "\"");
		throw Genode::Service_denied();
	}
}


bool Linux_network_session_base::_read_link_state_from_ndev() const
{
	return !(_ndev->state & (1UL << __LINK_STATE_NOCARRIER));
}


void Linux_network_session_base::unblock_rx_task(napi_struct * n)
{
	_rx_data.napi = n;
	_rx_task.unblock();
}
