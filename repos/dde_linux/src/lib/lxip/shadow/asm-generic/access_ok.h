/*
 * \brief  This file shadows asm-generic/acess_ok.h
 * \author Sebastian Sumpf
 * \date   2024-03-26
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SHADOW__ASM_GENERIC_ACCESS_OK_H_
#define _SHADOW__ASM_GENERIC_ACCESS_OK_H_

/*
 * The IP stack checks "user" pointer access, for example, for iov's using
 * 'access_ok' which in turn calls '__access_ok'. The function checks if the
 * pointer is below TASK_SIZE_MAX, which is usually a big value on 64 bit
 * systems, but 3GB on 32 bit systems. Because the IP stack is mostly used with
 * Genode's libc, where pointers on some kernels (base-linux) can be >3GB and we
 * don't want to make an additional copy of each buffer/iov interacting with the
 * IP stack, we short circuit the function
 */
static inline int ___access_ok(const void *ptr, unsigned long size)
{
	return 1;
}

#define __access_ok ___access_ok

#include_next <asm-generic/access_ok.h>

#endif /* _SHADOW__ASM_GENERIC_ACCESS_OK_H_ */
