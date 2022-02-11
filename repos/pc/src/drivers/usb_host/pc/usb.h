/**
 * \brief  USB related definitions for kernel/genode c-api
 * \author Stefan Kalkowski
 * \date   2021-09-17
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <genode_c_api/usb.h>

struct usb_interface;

#ifdef __cplusplus
extern "C" {
#endif

struct genode_attached_dataspace *
genode_usb_allocate_peer_buffer(unsigned long size);

void genode_usb_free_peer_buffer(struct genode_attached_dataspace * ptr);

extern struct genode_usb_rpc_callbacks genode_usb_rpc_callbacks_obj;

#ifdef __cplusplus
}
#endif

