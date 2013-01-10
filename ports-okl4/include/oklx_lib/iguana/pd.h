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

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__PD_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__PD_H_

#include <iguana/types.h>

typedef int pd_ref_t; /* protection domain id */

/**
 * Get my protection domain
 *
 * \return protection domain id
 */
pd_ref_t pd_myself(void);


/**
 * Delete protection domain
 *
 * \param pd protection domain id
 */
void pd_delete(pd_ref_t pd);

/**
 * Create memory area for protection domain
 *
 * \param pd   protection domain id
 * \param size size of new memory area
 * \param base base address of new memory area
 * \return     memory area id
 */
memsection_ref_t pd_create_memsection(pd_ref_t pd, uintptr_t size,
                                      uintptr_t *base);

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__PD_H_
