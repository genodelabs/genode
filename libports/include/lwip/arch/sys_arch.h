/*
 * \brief  Platform definitions for certain types in LwIP.
 * \author Stefan Kalkowski
 * \date   2009-11-10
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __LWIP__ARCH__SYS_ARCH_H__
#define __LWIP__ARCH__SYS_ARCH_H__

#include <arch/cc.h>

struct _sys_sem_t {
	void* ptr;
};
typedef struct _sys_sem_t sys_sem_t;

struct _sys_mbox_t {
	void* ptr;
};
typedef struct _sys_mbox_t sys_mbox_t;

typedef mem_ptr_t sys_thread_t;
typedef mem_ptr_t sys_prot_t;

#define SYS_MBOX_NULL 0
#define SYS_SEM_NULL  0

sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);

#endif /* __LWIP__ARCH__SYS_ARCH_H__ */
