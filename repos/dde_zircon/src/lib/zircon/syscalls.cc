/*
 * \brief  Definition of Zircon system calls
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <io_port_session/connection.h>

#include <zircon/types.h>
#include <ddk/device.h>
#include <ddk/driver.h>

#include <zx/static_resource.h>
#include <zx/device.h>

extern "C" {

	zx_handle_t get_root_resource()
	{
		return 0;
	}

	zx_status_t zx_handle_close(zx_handle_t)
	{
		return ZX_OK;
	}

	zx_status_t device_add_from_driver(zx_driver_t *,
	                                   zx_device_t *,
	                                   device_add_args_t *args,
	                                   zx_device_t **)
	{
		ZX::Device &dev = ZX::Resource<ZX::Device>::get_component();
		hidbus_ifc_t *hidbus;
		if (dev.hidbus(&hidbus)){
			static_cast<hidbus_protocol_ops_t *>(args->proto_ops)->start(
				args->ctx, hidbus, dev.component());
		}
		return ZX_OK;
	}

}
