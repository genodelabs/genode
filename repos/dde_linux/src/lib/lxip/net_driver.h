/*
 * \brief  Nic client that transfers packets from/to IP stack to/from nic client
 *         C-API
 * \author Sebastian Sumpf
 * \date   2024-01-29
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef _NET_DRIVER_H_
#define _NET_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif
	/* net_driver.c */
	struct task_struct;
	struct task_struct *lx_nic_client_rx_task(void);
	bool                lx_nic_client_link_state(void);
	bool                lx_nic_client_update_link_state(void);

	/* socket.cc */
	void socket_schedule_peer(void);
	void socket_config_address(void);
	void socket_unconfigure_address(void);
	void socket_update_link_state(void);

	void socket_label(char const *);
	char const *socket_nic_client_label(void);

#ifdef __cplusplus
}
#endif

#endif /* _NET_DRIVER_H_ */
