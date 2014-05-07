/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__IO__IO_H_
#define _L4__IO__IO_H_

#include <l4/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

enum l4io_resource_types_t {
	L4IO_RESOURCE_INVALID = 0,
	L4IO_RESOURCE_IRQ,
	L4IO_RESOURCE_MEM,
	L4IO_RESOURCE_PORT,
	L4IO_RESOURCE_ANY = ~0
};

/**
 * Flags for 'l4io_request_iomem'
 *
 * These flags are specified by 'map_dma_mem' in 'arch-arm/mach_setup.c'.
 */
enum {
	L4IO_MEM_NONCACHED = 1 << 0,
	L4IO_MEM_EAGER_MAP = 1 << 1
};

typedef l4_mword_t l4io_device_handle_t;
typedef int        l4io_resource_handle_t;
typedef l4_addr_t  l4vbus_paddr_t;

typedef struct {
	int      type;
	char     name[64];
	unsigned num_resources;
	unsigned flags;
} l4io_device_t;

typedef struct {
	l4_uint16_t type;
	l4_uint16_t flags;
	l4_addr_t   start;
	l4_addr_t   end;
} l4io_resource_t;

l4io_device_handle_t l4io_get_root_device(void);

L4_CV int L4_EXPORT
l4io_iterate_devices(l4io_device_handle_t *devhandle,
                     l4io_device_t *dev, l4io_resource_handle_t *reshandle);

L4_CV int L4_EXPORT
l4io_lookup_resource(l4io_device_handle_t devhandle,
                     enum l4io_resource_types_t type,
                     l4io_resource_handle_t *reshandle,
                     l4io_resource_t *res);

L4_CV long L4_EXPORT
l4io_request_ioport(unsigned portnum, unsigned len);

L4_CV long L4_EXPORT
l4io_request_iomem_region(l4_addr_t phys, l4_addr_t virt,
                          unsigned long size, int flags);

L4_CV long L4_EXPORT
l4io_search_iomem_region(l4_addr_t phys, l4_addr_t size,
                         l4_addr_t *rstart, l4_addr_t *rsize);

L4_CV long L4_EXPORT
l4io_request_iomem(l4_addr_t phys, unsigned long size, int flags,
                   l4_addr_t *virt);

L4_CV long L4_EXPORT
l4io_release_iomem(l4_addr_t virt, unsigned long size);

L4_CV long L4_EXPORT
l4io_request_irq(int irqnum, l4_cap_idx_t irqcap);

L4_CV long L4_EXPORT
l4io_release_irq(int irqnum, l4_cap_idx_t irq_cap);

L4_CV int L4_EXPORT
l4io_has_resource(enum l4io_resource_types_t type,
                  l4vbus_paddr_t start, l4vbus_paddr_t end);

#ifdef __cplusplus
}
#endif

#endif /* _L4__IO__IO_H_ */
