/*
 * \brief  DDE iPXE NIC API
 * \author Christian Helmuth
 * \date   2010-09-05
 *
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DDE_IPXE__NIC_H_
#define _DDE_IPXE__NIC_H_

/**
 * Packet reception callback
 *
 * \param   if_index    index of the receiving network interface
 * \param   packet      buffer containing the packet
 * \param   packet_len  packet length
 */
typedef void (*dde_ipxe_nic_rx_cb)(unsigned if_index, const char *packet, unsigned packet_len);

/**
 * Register packet reception callback
 *
 * \param   cb   new callback function
 *
 * \return  old callback function pointer
 *
 * This registers a function pointer as rx callback. Incoming ethernet packets
 * are passed to this function.
 */
extern dde_ipxe_nic_rx_cb dde_ipxe_nic_register_rx_callback(dde_ipxe_nic_rx_cb cb);

/**
 * Send packet
 *
 * \param   if_index    index of the network interface to be used for sending
 * \param   packet      buffer containing the packet
 * \param   packet_len  packet length
 *
 * \return  0 on success, -1 otherwise
 */
extern int dde_ipxe_nic_tx(unsigned if_index, const char *packet, unsigned packet_len);

/**
 * Get MAC address of device
 *
 * \param   if_index      index of the network interface
 * \param   out_mac_addr  buffer for MAC address (buffer size must be 6 byte)
 *
 * \return  0 on success, -1 otherwise
 */
extern int dde_ipxe_nic_get_mac_addr(unsigned if_index, char *out_mac_addr);

/**
 * Initialize network sub-system
 *
 * \return  number of network devices
 */
extern int dde_ipxe_nic_init(void);

#endif /* _DDE_IPXE__NIC_H_ */
