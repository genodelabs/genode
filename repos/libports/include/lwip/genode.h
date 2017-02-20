/*
 * \brief  Genode-specific lwIP API
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2010-02-22
 *
 * This header is included from C sources.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef __LWIP__GENODE_H__
#define __LWIP__GENODE_H__

#include <base/fixed_stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the TCP/IP stack */
void lwip_tcpip_init(void);

/**
 * Initialize lwIP for NIC session
 *
 * \param ip_addr  IPv4 address in network byte order, or zero when requesting
 *                 DHCP
 * \param netmask  IPv4 network mask in network byte order
 * \param gateway  IPv4 network-gateway address in network byte order
 * \param tx_buf_size  packet stream buffer size for TX direction
 * \param rx_buf_size  packet stream buffer size for RX direction
 *
 * \return 0 on success, or 1 if DHCP failed.
 *
 * This function initializes Genode's nic_drv and does optionally send DHCP
 * requests.
 */
int lwip_nic_init(genode_int32_t ip_addr,
                  genode_int32_t netmask,
                  genode_int32_t gateway,
                  unsigned long  tx_buf_size,
                  unsigned long  rx_buf_size);

/**
 * Pass on link-state changes to lwIP
 *
 * \param state current link-state
 */
void lwip_nic_link_state_changed(int state);

#ifdef __cplusplus
}
#endif

#endif /* __LWIP__GENODE_H__ */
