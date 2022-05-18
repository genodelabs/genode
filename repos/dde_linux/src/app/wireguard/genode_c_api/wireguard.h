/*
 * \brief  Glue code between Genode C++ code and Wireguard C code
 * \author Martin Stein
 * \date   2022-01-07
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENODE_C_API__WIREGUARD_H_
#define _GENODE_C_API__WIREGUARD_H_

enum { GENODE_WG_KEY_LEN = 32 };

typedef unsigned char  genode_wg_u8_t;
typedef unsigned short genode_wg_u16_t;
typedef unsigned int   genode_wg_u32_t;
typedef unsigned long  genode_wg_size_t;

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*genode_wg_config_add_dev_t)
	(genode_wg_u16_t listen_port, const genode_wg_u8_t * const priv_key);

typedef void (*genode_wg_config_rm_dev_t) (genode_wg_u16_t listen_port);

typedef void (*genode_wg_config_add_peer_t) (
	genode_wg_u16_t             listen_port,
	genode_wg_u8_t const        endpoint_ip[4],
	genode_wg_u16_t             endpoint_port,
	genode_wg_u8_t const *const pub_key,
	genode_wg_u8_t const        allowed_ip_addr[4],
	genode_wg_u8_t const        allowed_ip_prefix
);

typedef void (*genode_wg_config_rm_peer_t)
	(genode_wg_u16_t listen_port, genode_wg_u8_t const endpoint_ip[4],
	 genode_wg_u16_t endpoint_port);


struct genode_wg_config_callbacks
{
	genode_wg_config_add_dev_t  add_device;
	genode_wg_config_rm_dev_t   remove_device;
	genode_wg_config_add_peer_t add_peer;
	genode_wg_config_rm_peer_t  remove_peer;
};


void genode_wg_update_config(struct genode_wg_config_callbacks * callbacks);


typedef
void (*genode_wg_uplink_connection_receive_t)(void             *buf_base,
                                              genode_wg_size_t  buf_size);


typedef void (*genode_wg_nic_connection_receive_t)(void             *buf_base,
                                                   genode_wg_size_t  buf_size);


void
genode_wg_net_receive(genode_wg_uplink_connection_receive_t uplink_rx_callback,
                      genode_wg_nic_connection_receive_t    nic_rx_callback);


genode_wg_u16_t genode_wg_listen_port(void);


void genode_wg_send_wg_prot_at_nic_connection(
	genode_wg_u8_t const *wg_prot_base,
	genode_wg_size_t      wg_prot_size,
	genode_wg_u16_t       udp_src_port_big_endian,
	genode_wg_u16_t       udp_dst_port_big_endian,
	genode_wg_u32_t       ipv4_src_addr_big_endian,
	genode_wg_u32_t       ipv4_dst_addr_big_endian,
	genode_wg_u8_t        ipv4_dscp_ecn,
	genode_wg_u8_t        ipv4_ttl);


void genode_wg_send_ip_at_uplink_connection(
	genode_wg_u8_t const *ip_base,
	genode_wg_size_t      ip_size);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__WIREGUARD_H_ */
