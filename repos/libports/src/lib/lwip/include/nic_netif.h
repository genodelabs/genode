/*
 * \brief  Network interface handling
 * \author Sebastian Sumpf
 * \date   2025-02-27
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC_NETIF_H_
#define _INCLUDE__NIC_NETIF_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct genode_netif_handle;
struct genode_socket_config;
struct genode_socket_info;

struct genode_netif_handle *lwip_genode_netif_init(char const *label);

void lwip_genode_netif_address(struct genode_netif_handle  *handle,
                               struct genode_socket_config *config);
void lwip_genode_netif_info(struct genode_netif_handle *handle,
                            struct genode_socket_info  *info);
void lwip_genode_netif_mtu(struct genode_netif_handle *handle, unsigned mtu);
bool lwip_genode_netif_configured(struct genode_netif_handle *handle);
void lwip_genode_netif_link_state(struct genode_netif_handle *handle);
void lwip_genode_netif_rx(struct genode_netif_handle *handle);

void lwip_genode_socket_schedule_peer(void);


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _INCLUDE__NIC_NETIF_H_ */

