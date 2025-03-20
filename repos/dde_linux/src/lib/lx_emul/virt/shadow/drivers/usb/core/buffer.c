/*
 * \brief  HCD-memory management
 * \author Stefan Kalkowski
 * \date   2023-09-14
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#include <linux/usb.h>
#include <linux/usb/hcd.h>


void * hcd_buffer_alloc(struct usb_bus * bus, size_t size, gfp_t mem_flags, dma_addr_t * dma)
{
	return kmalloc(size, GFP_KERNEL);
}


void hcd_buffer_free(struct usb_bus * bus, size_t size, void * addr, dma_addr_t dma)
{
	kfree(addr);
}
