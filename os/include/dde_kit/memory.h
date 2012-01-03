/*
 * \brief   Memory subsystem
 * \author  Christian Helmuth
 * \date    2008-08-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__MEMORY_H_
#define _INCLUDE__DDE_KIT__MEMORY_H_

#include <dde_kit/types.h>


/*******************
 ** Slab facility **
 *******************/

struct dde_kit_slab;

/**
 * Store user pointer in slab cache
 *
 * \param   slab  pointer to slab cache
 * \param   data  user pointer
 */
void dde_kit_slab_set_data(struct dde_kit_slab * slab, void *data);

/**
 * Read user pointer from slab cache
 *
 * \param   slab  pointer to slab cache
 *
 * \return  stored user pointer or 0
 */
void *dde_kit_slab_get_data(struct dde_kit_slab * slab);

/**
 * Allocate slab in slab cache
 *
 * \param   slab  pointer to slab cache
 *
 * \return  pointer to allocated slab
 */
void *dde_kit_slab_alloc(struct dde_kit_slab * slab);

/**
 * Deallocate slab in slab cache
 *
 * \param   slab  pointer to slab cache
 * \param   objp  pointer to allocated slab
 */
void dde_kit_slab_free(struct dde_kit_slab * slab, void *objp);

/**
 * Destroy slab cache
 *
 * \param   slab  pointer to slab cache structure
 */
void dde_kit_slab_destroy(struct dde_kit_slab * slab);

/**
 * Initialize slab cache
 *
 * \param   size  size of cache objects
 *
 * \return  pointer to new slab cache or 0 on error
 *
 * Allocated blocks have valid virt->phys mappings and are physically
 * contiguous.
 */
struct dde_kit_slab * dde_kit_slab_init(unsigned size);


/**********************************
 ** Large-block memory allocator **
 **********************************/

/**
 * Allocate large memory block
 *
 * \param   size  block size
 *
 * \return  pointer to new memory block
 *
 * Allocations via this allocator may be slow (because RPCs to remote services
 * may be involved) and should be used only for large blocks of several pages.
 * If allocations/deallocations are relatively dynamic the large memory
 * allocator should be used as backend for a block caching frontend.
 *
 * Allocated blocks have valid virt->phys mappings and are physically
 * contiguous.
 */
void *dde_kit_large_malloc(dde_kit_size_t size);

/**
 * Free large memory block
 *
 * \param   p  pointer to memory block
 */
void  dde_kit_large_free(void *p);


/*****************************
 ** Simple memory allocator **
 *****************************/

/**
 * Allocate memory block via simple allocator
 *
 * \param   size  block size
 *
 * \return  pointer to new memory block
 *
 * The blocks allocated via this allocator CANNOT be used for DMA or other
 * device operations, i.e., there exists no virt->phys mapping.
 */
void *dde_kit_simple_malloc(dde_kit_size_t size);

/**
 * Free memory block via simple allocator
 *
 * \param   p  pointer to memory block
 *
 * As in C99, if 'p' is NULL no operation is performed.
 */
void dde_kit_simple_free(void *p);

#endif /* _INCLUDE__DDE_KIT__MEMORY_H_ */
