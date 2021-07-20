/*
 * \brief  C interface to Genode's uplink session
 * \author Norman Feske
 * \date   2021-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GENODE_C_API__UPLINK_H_
#define _INCLUDE__GENODE_C_API__UPLINK_H_

#include <genode_c_api/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize uplink handling
 *
 * \param sigh  signal handler to be installed at the uplink connection
 */
void genode_uplink_init(struct genode_env *,
                        struct genode_allocator *,
                        struct genode_signal_handler *);

/**
 * Wake up uplink server if progress can be made at the server side
 *
 * This function should be called whenever the component becomes idle.
 */
void genode_uplink_notify_peers(void);



struct genode_uplink;  /* definition is private to the implementation */


/*********************
 ** Uplink lifetime **
 *********************/

struct genode_uplink_args
{
	unsigned char mac_address[6];
	char const *label;
};

struct genode_uplink *genode_uplink_create(struct genode_uplink_args const *);

void genode_uplink_destroy(struct genode_uplink *);


/*************************************************
 ** Transmit packets towards the uplink session **
 *************************************************/

struct genode_uplink_tx_packet_context;

/**
 * Callback called by 'genode_uplink_tx_packet' to provide the content
 */
typedef unsigned long (*genode_uplink_tx_packet_content_t)
	(struct genode_uplink_tx_packet_context *, char *dst, unsigned long dst_len);

/**
 * Process packet transmission
 *
 * \return true if progress was made
 */
bool genode_uplink_tx_packet(struct genode_uplink *,
                             genode_uplink_tx_packet_content_t,
                             struct genode_uplink_tx_packet_context *);


/*********************************************
 ** Receive packets from the uplink session **
 *********************************************/

struct genode_uplink_rx_context;

typedef enum { GENODE_UPLINK_RX_REJECTED,
               GENODE_UPLINK_RX_ACCEPTED,
               GENODE_UPLINK_RX_RETRY } genode_uplink_rx_result_t;

typedef genode_uplink_rx_result_t (*genode_uplink_rx_one_packet_t)
	(struct genode_uplink_rx_context *, char const *ptr, unsigned long len);

/**
 * Process packet reception
 *
 * \return true if progress was made
 */
bool genode_uplink_rx(struct genode_uplink *,
                      genode_uplink_rx_one_packet_t rx_one_packet,
                      struct genode_uplink_rx_context *);

#ifdef __cplusplus
}
#endif

#endif /* _INCLUDE__GENODE_C_API__UPLINK_H_ */
