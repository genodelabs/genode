/*
 * \brief  DDE iPXE NIC API
 * \author Christian Helmuth
 * \date   2010-09-05
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _DDE_IPXE__NIC_H_
#define _DDE_IPXE__NIC_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Link-state change callback
 */
typedef void (*dde_ipxe_nic_link_cb)(void);

/**
 * Packet reception callback
 *
 * \param   if_index    index of the receiving network interface
 * \param   packet      buffer containing the packet
 * \param   packet_len  packet length
 */
typedef void (*dde_ipxe_nic_rx_cb)(unsigned if_index, const char *packet, unsigned packet_len);

/**
 * Notification about that all packets have been received
 */
typedef void (*dde_ipxe_nic_rx_done)(void);

/**
 * Register packet reception callback
 *
 * \param   rx_cb    packet-reception callback function
 * \param   link_cb  link-state change callback function
 * \param   rx_done  all packets are received callback function
 *
 * This registers a function pointer as rx callback. Incoming ethernet packets
 * are passed to this function.
 */
extern void dde_ipxe_nic_register_callbacks(dde_ipxe_nic_rx_cb,
                                            dde_ipxe_nic_link_cb,
                                            dde_ipxe_nic_rx_done);

/**
 * Clear callbacks
 */
extern void dde_ipxe_nic_unregister_callbacks();


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
extern int dde_ipxe_nic_get_mac_addr(unsigned if_index, unsigned char *out_mac_addr);

/**
 * Get current link-state of device
 *
 * \param  if_index  index of the receiving network interface
 *
 * \return 1 if link is up, 0 if no link is detected
 */
extern int dde_ipxe_nic_link_state(unsigned if_index);

/**
 * Initialize network sub-system
 *
 * \return  number of network devices
 */
extern int dde_ipxe_nic_init();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _DDE_IPXE__NIC_H_ */
