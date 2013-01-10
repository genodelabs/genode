/*
 * \brief  Iguana API functions needed by OKLinux
 * \author Norman Feske
 * \date   2009-04-12
 *
 * This file contains function definitions to create/destroy address spaces,
 * threads within other address spaces and to populate the address spaces
 * with pages.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__EAS_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__EAS_H_

#include <iguana/types.h>

typedef uintptr_t eas_ref_t; /* Address space id corresponds to OKL4 space id*/

/**
 * Create a thread within another address space
 *
 * \param eas       identifier of the target address space
 * \param pager     thread id of the new thread's pager
 * \param scheduler thread id of the new thread's scheduler
 * \param utcb      address of utcb of the new thread
 * \param handle_rv resulting pointer to the id of the new thread
 * \return          id of the new thread
 */
L4_ThreadId_t eas_create_thread(eas_ref_t eas, L4_ThreadId_t pager,
                                L4_ThreadId_t scheduler, void *utcb,
                                L4_ThreadId_t *handle_rv);

/**
 * Create a new address space
 *
 * \param utcb  defines utcb region of the address space
 * \param l4_id resulting pointer to the id of the new address space
 * \return      id of the new address space
 */
eas_ref_t eas_create(L4_Fpage_t utcb, L4_SpaceId_t *l4_id);

/**
 * Deletes an address space
 *
 * \param eas identifier of the target address space
 */
void eas_delete(eas_ref_t eas);

/**
 * Map a page area to an address space
 *
 * \param eas        identifier of the target address space
 * \param src_fpage  flexpage describing the area to be mapped
 * \param dst_addr   destination address where the area will be mapped to
 * \param attributes describes the caching policy (OKL4 manual)
 */
int eas_map(eas_ref_t eas, L4_Fpage_t src_fpage,
            uintptr_t dst_addr, uintptr_t attributes);

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__EAS_H_
