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

#ifndef _INCLUDE__OKLINUX_SUPPORT__IGUANA__HARDWARE_H_
#define _INCLUDE__OKLINUX_SUPPORT__IGUANA__HARDWARE_H_

#include <iguana/types.h>

/**
 * Register thread as interrupt handler
 *
 * \param thrd      thread id of the interrupt handler
 * \param interrupt interrupt number
 * \return          0 if no error occured
 */
int hardware_register_interrupt(L4_ThreadId_t thrd, int interrupt);

/**
 * Register physical memory region
 *
 * \param memsection describes virtual memory area
 * \param paddr      physical address backing the memory area
 * \param attributes caching policy
 * \return           0 if no error occured
 */
int hardware_back_memsection(memsection_ref_t memsection,
                             uintptr_t paddr, uintptr_t attributes);

#endif //_INCLUDE__OKLINUX_SUPPORT__IGUANA__HARDWARE_H_
