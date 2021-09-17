/*
 * \brief  C-API Genode USB backend
 * \author Stefan Kalkowski
 * \date   2021-09-14
 */

/*
 * Copyright (C) 2006-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENODE_C_API__USB_H_
#define _GENODE_C_API__USB_H_

#include <genode_c_api/base.h>


struct genode_usb_session; /* definition is private to the implementation */


#ifdef __cplusplus
extern "C" {
#endif


/********************
 ** Initialization **
 ********************/

/**
 * Callback called during peer session request to allocate dma-capable shared buffer
 */
typedef struct genode_attached_dataspace * (*genode_usb_alloc_peer_buffer_t)
	(unsigned long size);

/**
 * Callback called when closing peer session to free shared buffer
 */
typedef void (*genode_usb_free_peer_buffer_t)
	(struct genode_attached_dataspace * ds);

/**
 * Callback to copy over config descriptor for given device
 */
typedef unsigned (*genode_usb_rpc_config_desc_t)
	(unsigned long bus, unsigned long dev, void * dev_desc, void * conf_desc);

/**
 * Callback that returns number of alt-settings of an interface for given device
 */
typedef int (*genode_usb_rpc_alt_settings_t)
	(unsigned long bus, unsigned long dev, unsigned idx);

/**
 * Callback to copy over interface descriptor for given device/interface
 */
typedef int (*genode_usb_rpc_iface_desc_t)
	(unsigned long bus, unsigned long dev, unsigned idx, unsigned alt,
     void * buf, unsigned long buf_size, int * active);

/**
 * Callback to copy over additional vendor specific data of an interface
 */
typedef int (*genode_usb_rpc_iface_extra_t)
	(unsigned long bus, unsigned long dev, unsigned idx, unsigned alt,
     void * buf, unsigned long buf_size);

/**
 * Callback to copy over endpoint descriptor for given device/iface/endpoint
 */
typedef int (*genode_usb_rpc_endp_desc_t)
	(unsigned long bus, unsigned long dev, unsigned idx, unsigned alt,
     unsigned endp, void * buf, unsigned long buf_size);

struct genode_usb_rpc_callbacks {
	genode_usb_alloc_peer_buffer_t alloc_fn;
	genode_usb_free_peer_buffer_t  free_fn;
	genode_usb_rpc_config_desc_t   cfg_desc_fn;
	genode_usb_rpc_alt_settings_t  alt_settings_fn;
	genode_usb_rpc_iface_desc_t    iface_desc_fn;
	genode_usb_rpc_iface_extra_t   iface_extra_fn;
	genode_usb_rpc_endp_desc_t     endp_desc_fn;
};

/**
 * Initialize USB root component
 *
 * \param handler  signal handler to be installed at each USB session
 */
void genode_usb_init(struct genode_env               * env,
                     struct genode_allocator         * alloc,
                     struct genode_signal_handler    * handler,
                     struct genode_usb_rpc_callbacks * callbacks);


/************************************
 ** USB device lifetime management **
 ************************************/

void genode_usb_announce_device(unsigned long vendor,
                                unsigned long product,
                                unsigned long cla,
                                unsigned long bus,
                                unsigned long dev);

void genode_usb_discontinue_device(unsigned long bus, unsigned long dev);


/**********************************
 ** USB session request handling **
 **********************************/

typedef unsigned short genode_usb_session_handle_t;
typedef unsigned short genode_usb_request_handle_t;

struct genode_usb_request_string
{
	unsigned char index;
	unsigned      length;
};

struct genode_usb_request_control
{
	unsigned char  request;
	unsigned char  request_type;
	unsigned short value;
	unsigned short index;
	int            actual_size;
	int            timeout;
};

enum Iso  { MAX_PACKETS = 32 };
enum Transfer { BULK, IRQ, ISOC };
typedef enum Transfer genode_usb_transfer_type_t;

struct genode_usb_request_transfer
{
	unsigned char ep;
	int           actual_size;
	int           polling_interval;
	int           number_of_packets;
	unsigned long packet_size[MAX_PACKETS];
	unsigned long actual_packet_size[MAX_PACKETS];
};

enum Request_return_error {
	NO_ERROR,
	INTERFACE_OR_ENDPOINT_ERROR,
	MEMORY_ERROR,
	NO_DEVICE_ERROR,
	PACKET_INVALID_ERROR,
	PROTOCOL_ERROR,
	STALL_ERROR,
	TIMEOUT_ERROR,
	UNKNOWN_ERROR
};
typedef enum Request_return_error genode_usb_request_ret_t;

typedef genode_usb_request_ret_t (*genode_usb_req_ctrl_t)
	(struct genode_usb_request_control * req,
	 void                              * payload,
	 unsigned long                       payload_size,
	 void                              * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_req_transfer_t)
	(struct genode_usb_request_transfer * req,
	 genode_usb_transfer_type_t           type,
	 genode_usb_session_handle_t          session_handle,
	 genode_usb_request_handle_t          request_handle,
	 void                               * payload,
	 unsigned long                        payload_size,
	 void                               * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_req_string_t)
	(struct genode_usb_request_string * req,
	 void                             * payload,
	 unsigned long                      payload_size,
	 void                             * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_req_altsetting_t)
	(unsigned iface, unsigned alt_setting, void * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_req_config_t)
	(unsigned config_idx, void * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_req_flush_t)
	(unsigned char ep, void * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_response_t)
	(struct genode_usb_request_transfer * req,
	 void                               * opaque_data);

struct genode_usb_request_callbacks {
	genode_usb_req_ctrl_t       control_fn;
	genode_usb_req_transfer_t   transfer_fn;
	genode_usb_req_string_t     string_fn;
	genode_usb_req_altsetting_t altsetting_fn;
	genode_usb_req_config_t     config_fn;
	genode_usb_req_flush_t      flush_fn;
};

genode_usb_session_handle_t genode_usb_session_by_bus_dev(unsigned long bus,
                                                          unsigned long dev);

int genode_usb_request_by_session(genode_usb_session_handle_t session_handle,
                                  struct genode_usb_request_callbacks * const c,
                                  void * callback_data);

void genode_usb_ack_request(genode_usb_session_handle_t session_handle,
                            genode_usb_request_handle_t request_handle,
                            genode_usb_response_t       callback,
                            void *                      callback_data);

void genode_usb_notify_peers(void);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__USB_H_ */

