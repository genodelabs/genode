/**
 * \brief  Nic client that transfers packets from/to IP stack to/from nic client
 *         C-API
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _NET_DRIVER_H_
#define _NET_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif
	struct task_struct;
	struct task_struct *lx_nic_client_rx_task(void);
#ifdef __cplusplus
}
#endif

#endif /* _NET_DRIVER_H_ */
