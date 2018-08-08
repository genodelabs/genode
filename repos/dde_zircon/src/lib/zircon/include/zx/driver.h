/*
 * \brief  Helper for Zircon driver start mechanism
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ZX_DRIVER_H_
#define _ZX_DRIVER_H_

#include <base/log.h>

#include <ddk/driver.h>

extern zx_driver_rec_t __zircon_driver_rec__;

static inline int bind_driver(void *ctx, zx_device_t* parent)
{
	int ret = -1;
	if (__zircon_driver_rec__.ops->version == DRIVER_OPS_VERSION){
		ret = __zircon_driver_rec__.ops->bind(ctx, parent);
	}else{
		Genode::error("Failed to start driver, invalid DRIVER_OPS_VERSION ",
		              Genode::Hex(__zircon_driver_rec__.ops->version));
	}
	return ret;
}

#endif /* ifndef _ZX_DRIVER_H_ */
