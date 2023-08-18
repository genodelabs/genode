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

typedef unsigned short genode_usb_vendor_id_t;
typedef unsigned short genode_usb_product_id_t;
typedef unsigned int   genode_usb_bus_num_t;
typedef unsigned int   genode_usb_dev_num_t;
typedef unsigned char  genode_usb_class_num_t;

#ifdef __cplusplus
extern "C" {
#endif


/********************
 ** Initialization **
 ********************/

/**
 * Callback to copy over config descriptor for given device
 */
typedef unsigned (*genode_usb_rpc_config_desc_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev,
	 void * dev_desc, void * conf_desc);

/**
 * Callback that returns number of alt-settings of an interface for given device
 */
typedef int (*genode_usb_rpc_alt_settings_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev, unsigned idx);

/**
 * Callback to copy over interface descriptor for given device/interface
 */
typedef int (*genode_usb_rpc_iface_desc_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev, unsigned idx,
	 unsigned alt, void * buf, unsigned long buf_size, int * active);

/**
 * Callback to copy over additional vendor specific data of an interface
 */
typedef int (*genode_usb_rpc_iface_extra_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev, unsigned idx,
	 unsigned alt, void * buf, unsigned long buf_size);

/**
 * Callback to copy over endpoint descriptor for given device/iface/endpoint
 */
typedef int (*genode_usb_rpc_endp_desc_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev, unsigned idx,
	 unsigned alt, unsigned endp, void * buf, unsigned long buf_size);

/**
 * Callback to claim a given interface
 */
typedef int (*genode_usb_rpc_claim_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev, unsigned iface);

/**
 * Callback to release a given interface
 */
typedef int (*genode_usb_rpc_release_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev, unsigned iface);

/**
 * Callback to release all interfaces
 */
typedef void (*genode_usb_rpc_release_all_t)
	(genode_usb_bus_num_t bus, genode_usb_dev_num_t dev);

struct genode_usb_rpc_callbacks {
	genode_shared_dataspace_alloc_attach_t alloc_fn;
	genode_shared_dataspace_free_t         free_fn;
	genode_usb_rpc_config_desc_t           cfg_desc_fn;
	genode_usb_rpc_alt_settings_t          alt_settings_fn;
	genode_usb_rpc_iface_desc_t            iface_desc_fn;
	genode_usb_rpc_iface_extra_t           iface_extra_fn;
	genode_usb_rpc_endp_desc_t             endp_desc_fn;
	genode_usb_rpc_claim_t                 claim_fn;
	genode_usb_rpc_release_t               release_fn;
	genode_usb_rpc_release_all_t           release_all_fn;
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

void genode_usb_announce_device(genode_usb_vendor_id_t  vendor,
                                genode_usb_product_id_t product,
                                genode_usb_class_num_t  cla,
                                genode_usb_bus_num_t    bus,
                                genode_usb_dev_num_t    dev);

void genode_usb_discontinue_device(genode_usb_bus_num_t bus,
                                   genode_usb_dev_num_t dev);


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

struct genode_usb_request_transfer
{
	unsigned char ep;
	int           actual_size;
	int           polling_interval;
};

enum Isoc { MAX_PACKETS = 32 };

struct genode_usb_isoc_transfer
{
	unsigned number_of_packets;
	unsigned packet_size[MAX_PACKETS];
	unsigned actual_packet_size[MAX_PACKETS];
	char     data[];
};

enum Urb_type { CTRL, BULK, IRQ, ISOC, ALT_SETTING, CONFIG, NONE };
typedef enum Urb_type genode_usb_urb_t;

struct genode_usb_request_urb
{
	genode_usb_urb_t type;
	void           * req;
};

struct genode_usb_buffer
{
	void        * addr;
	unsigned long size;
};

static inline struct genode_usb_request_control *
genode_usb_get_request_control(struct genode_usb_request_urb * urb)
{
	return (urb->type == CTRL) ? (struct genode_usb_request_control*)urb->req : 0;
}

static inline struct genode_usb_request_transfer *
genode_usb_get_request_transfer(struct genode_usb_request_urb * urb)
{
	return (urb->type != CTRL) ? (struct genode_usb_request_transfer*)urb->req : 0;
}

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

typedef void (*genode_usb_req_urb_t)
	(struct genode_usb_request_urb req,
	 genode_usb_session_handle_t   session_handle,
	 genode_usb_request_handle_t   request_handle,
	 struct genode_usb_buffer      payload,
	 void                        * opaque_data);

typedef void (*genode_usb_req_string_t)
	(struct genode_usb_request_string * req,
	 genode_usb_session_handle_t        session_handle,
	 genode_usb_request_handle_t        request_handle,
	 struct genode_usb_buffer           payload,
	 void                             * opaque_data);

typedef void (*genode_usb_req_altsetting_t)
	(unsigned iface, unsigned alt_setting,
	 genode_usb_session_handle_t session_handle,
	 genode_usb_request_handle_t request_handle,
	 void * opaque_data);

typedef void (*genode_usb_req_config_t)
	(unsigned config_idx,
	 genode_usb_session_handle_t session_handle,
	 genode_usb_request_handle_t request_handle,
	 void * opaque_data);

typedef void (*genode_usb_req_flush_t)
	(unsigned char ep,
	 genode_usb_session_handle_t session_handle,
	 genode_usb_request_handle_t request_handle,
	 void * opaque_data);

typedef genode_usb_request_ret_t (*genode_usb_response_t)
	(struct genode_usb_request_urb req,
	 struct genode_usb_buffer      payload,
	 void                        * opaque_data);

struct genode_usb_request_callbacks {
	genode_usb_req_urb_t        urb_fn;
	genode_usb_req_string_t     string_fn;
	genode_usb_req_altsetting_t altsetting_fn;
	genode_usb_req_config_t     config_fn;
	genode_usb_req_flush_t      flush_fn;
};

genode_usb_session_handle_t genode_usb_session_by_bus_dev(genode_usb_bus_num_t bus,
                                                          genode_usb_dev_num_t dev);

int genode_usb_request_by_session(genode_usb_session_handle_t session_handle,
                                  struct genode_usb_request_callbacks * const c,
                                  void * callback_data);

void genode_usb_ack_request(genode_usb_session_handle_t session_handle,
                            genode_usb_request_handle_t request_handle,
                            genode_usb_response_t       callback,
                            void *                      callback_data);

void genode_usb_notify_peers(void);

void genode_usb_handle_empty_sessions(void);

#ifdef __cplusplus
}
#endif

#endif /* _GENODE_C_API__USB_H_ */
