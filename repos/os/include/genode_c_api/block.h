/*
 * \brief  C-API Genode block backend
 * \author Stefan Kalkowski
 * \date   2021-07-05
 */

/*
 * Copyright (C) 2006-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENODE_C_API__BLOCK_H_
#define _GENODE_C_API__BLOCK_H_

#include <genode_c_api/base.h>


struct genode_block_session; /* definition is private to the implementation */


#ifdef __cplusplus
extern "C" {
#endif


/********************
 ** Initialization **
 ********************/

/**
 * Callback called during peer session request to allocate dma-capable shared buffer
 */
typedef struct genode_attached_dataspace * (*genode_block_alloc_peer_buffer_t)
	(unsigned long size);

/**
 * Callback called when closing peer session to free shared buffer
 */
typedef void (*genode_block_free_peer_buffer_t)
	(struct genode_attached_dataspace * ds);

/**
 * Initialize block root component
 *
 * \param handler  signal handler to be installed at each block session
 */
void genode_block_init(struct genode_env            *env,
                       struct genode_allocator      *alloc,
                       struct genode_signal_handler *handler,
                       genode_block_alloc_peer_buffer_t,
                       genode_block_free_peer_buffer_t);


/**************************************
 ** Block device lifetime management **
 **************************************/

typedef unsigned long long genode_block_sector_t;

void genode_block_announce_device(const char *          name,
                                  genode_block_sector_t sectors,
                                  int                   writeable);

void genode_block_discontinue_device(const char * name);


/************************************
 ** Block session request handling **
 ************************************/

enum Operation {
	GENODE_BLOCK_READ,
	GENODE_BLOCK_WRITE,
	GENODE_BLOCK_SYNC,
	GENODE_BLOCK_UNAVAIL,
};

struct genode_block_request {
	int                   op;
	genode_block_sector_t blk_nr;
	genode_block_sector_t blk_cnt;
	void *                addr;
};

struct genode_block_session * genode_block_session_by_name(const char * name);

struct genode_block_request *
genode_block_request_by_session(struct genode_block_session * const session);

void genode_block_ack_request(struct genode_block_session * const session,
                              struct genode_block_request * const request,
                              int success);

void genode_block_notify_peers(void);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__BLOCK_H_ */
