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
#include <usb_session/types.h>


struct genode_usb_device;
struct genode_usb_configuration;
struct genode_usb_interface;

typedef unsigned int genode_usb_bus_num_t;
typedef unsigned int genode_usb_dev_num_t;

#ifdef __cplusplus
extern "C" {
#endif


/************************************
 ** USB device lifetime management **
 ************************************/


/**
 * Callback to announce configuration of a device
 */
typedef void (*genode_usb_dev_add_config_t)
	(struct genode_usb_device *dev, unsigned idx, void *opaque_data);

/**
 * Callback to announce interface of a device configuration
 */
typedef void (*genode_usb_dev_add_iface_t)
	(struct genode_usb_configuration *cfg, unsigned idx, void *opaque_data);

/**
 * Callback to announce endpoint of an interface
 */
typedef void (*genode_usb_dev_add_endp_t)
	(struct genode_usb_interface *cfg, unsigned idx, void *opaque_data);

/**
 * Callback to request string item of a device
 */
typedef void (*genode_usb_dev_string_item_t)
	(genode_buffer_t string, void *opaque_data);

void
genode_usb_device_add_endpoint(struct genode_usb_interface          *iface,
                               struct genode_usb_endpoint_descriptor desc);

void
genode_usb_device_add_interface(struct genode_usb_configuration       *cfg,
                                genode_usb_dev_string_item_t           info_string,
                                struct genode_usb_interface_descriptor desc,
                                genode_usb_dev_add_endp_t              callback,
                                void                                  *opaque_data,
                                bool                                   active);

void
genode_usb_device_add_configuration(struct genode_usb_device           *dev,
                                    struct genode_usb_config_descriptor desc,
                                    genode_usb_dev_add_iface_t          callback,
                                    void                               *opaque_data,
                                    bool                                active);

void genode_usb_announce_device(genode_usb_bus_num_t                bus,
                                genode_usb_dev_num_t                dev,
                                genode_usb_speed_t                  speed,
                                genode_usb_dev_string_item_t        manufacturer_string,
                                genode_usb_dev_string_item_t        product_string,
                                struct genode_usb_device_descriptor desc,
                                genode_usb_dev_add_config_t         callback,
                                void                               *opaque_data);

void genode_usb_discontinue_device(genode_usb_bus_num_t bus,
                                   genode_usb_dev_num_t dev);


/**********************************
 ** USB session request handling **
 **********************************/

typedef void* genode_usb_request_handle_t;

enum Request_return { OK, NO_DEVICE, INVALID, TIMEOUT, HALT, };
typedef enum Request_return genode_usb_request_ret_t;


typedef void (*genode_usb_req_control_t)
	(genode_usb_request_handle_t handle,
	 unsigned char               ctrl_request,
	 unsigned char               ctrl_request_type,
	 unsigned short              ctrl_value,
	 unsigned short              ctrl_index,
	 unsigned long               ctrl_timeout,
	 genode_buffer_t             payload,
	 void                       *opaque_data);

typedef void (*genode_usb_req_irq_t)
	(genode_usb_request_handle_t handle,
	 unsigned char               ep,
	 genode_buffer_t             payload,
	 void                       *opaque_data);

typedef void (*genode_usb_req_bulk_t)
	(genode_usb_request_handle_t handle,
	 unsigned char               ep,
	 genode_buffer_t             payload,
	 void                       *opaque_data);

typedef void (*genode_usb_req_isoc_t)
	(genode_usb_request_handle_t        handle,
	 unsigned char                      ep,
	 genode_uint32_t                    number_of_packets,
	 struct genode_usb_isoc_descriptor *packets,
	 genode_buffer_t                    payload,
	 void                              *opaque_data);

typedef void (*genode_usb_req_flush_t)
	(unsigned char               ep,
	 genode_usb_request_handle_t handle,
	 void                       *opaque_data);

struct genode_usb_request_callbacks {
	genode_usb_req_control_t ctrl_fn;
	genode_usb_req_irq_t     irq_fn;
	genode_usb_req_bulk_t    bulk_fn;
	genode_usb_req_isoc_t    isoc_fn;
	genode_usb_req_flush_t   flush_fn;
};

typedef struct genode_usb_request_callbacks * genode_usb_req_callback_t;

bool genode_usb_device_acquired(genode_usb_bus_num_t bus,
                                genode_usb_dev_num_t dev);

bool
genode_usb_request_by_bus_dev(genode_usb_bus_num_t            bus,
                              genode_usb_dev_num_t            dev,
                              genode_usb_req_callback_t const callback,
                              void                           *opaque_data);

void genode_usb_ack_request(genode_usb_request_handle_t request_handle,
                            genode_usb_request_ret_t    ret,
                            genode_uint32_t            *actual_sizes);

void genode_usb_notify_peers(void);

void genode_usb_handle_disconnected_sessions(void);

#ifdef __cplusplus
}
#endif


#ifdef __cplusplus

#include <base/env.h>
#include <base/signal.h>

/**
 * Callback to signal release of device(s)
 */
typedef void (*genode_usb_dev_release_t) (genode_usb_bus_num_t bus,
                                          genode_usb_dev_num_t dev);

namespace Genode_c_api {

	using namespace Genode;

	/**
	 * Initialize USB root component
	 */
	void initialize_usb_service(Env                                   &env,
	                            Signal_context_capability              sigh_cap,
	                            genode_shared_dataspace_alloc_attach_t alloc_fn,
	                            genode_shared_dataspace_free_t         free_fn,
	                            genode_usb_dev_release_t               release_fn);
}

#endif /* __cplusplus */

#endif /* _GENODE_C_API__USB_H_ */
