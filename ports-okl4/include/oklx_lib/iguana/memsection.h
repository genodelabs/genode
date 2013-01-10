/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Norman Feske
 * \date   2009-04-12
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__MEMSECTION_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__MEMSECTION_H_

#include <iguana/types.h>

/**
 * Get physical address of the given virtual one
 *
 * \param vaddr the virtual address
 * \param size  resulting pointer to the size of the corresponding area
 * \return      the requested physical address
 */
uintptr_t memsection_virt_to_phys(uintptr_t vaddr, size_t *size);

/**
 * Establish mapping to virtual memory area
 *
 * \param memsect   the target memory area
 * \param from_page flexpage describing the source memory area
 * \param to_page   flexpage describing the virtual memory area
 * \return          0 if no error occurred
 */
int memsection_page_map(memsection_ref_t memsect,
                        L4_Fpage_t from_page,
                        L4_Fpage_t to_page);

/**
 * Destroy mapping to virtual memory area
 *
 * \param memsect   the target memory area
 * \param to_page   flexpage describing the virtual memory area
 * \return          0 if no error occurred
 */
int memsection_page_unmap(memsection_ref_t memsect,
                          L4_Fpage_t to_page);

/**
 * Register pager of a memory area
 *
 * \param memsect   the target memory area
 * \param server    the pager's thread id
 * \return          0 if no error occurred
 */
int memsection_register_server(memsection_ref_t memsect,
                               thread_ref_t server);

/**
 * Lookup pager of an object
 *
 * \param object reference to object
 * \param server resulting pointer to object's pager
 * \return       memory area id, object is part of
 */
memsection_ref_t memsection_lookup(objref_t object, thread_ref_t *server);

/**
 * Get base address of a memory area
 *
 * \param memsect memory area id
 * \return        base address of the memory area
 */
void *memsection_base(memsection_ref_t memsect);

/**
 * Get size of a memory area
 *
 * \param memsect memory area id
 * \return        size of the memory area
 */
uintptr_t memsection_size(memsection_ref_t memsect);

/**
 * Create a blank memory area
 *
 * \param size size of the area
 * \param base base address of the area
 * \return     memory area id
 */
memsection_ref_t memsection_create_user(uintptr_t size, uintptr_t *base);

/**
 * Delete a memory area
 *
 * \param memsect memory area id
 */
void memsection_delete(memsection_ref_t);

/**
 * Create a blank memory area
 *
 * \param size    size of the area
 * \param base    base address of the area
 * \param memsect memory area id
 */
memsection_ref_t memsection_create(uintptr_t size, uintptr_t *base);

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__MEMSECTION_H_
