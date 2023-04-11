/*
 * \brief  Implementation for seL4_GetIPCBuffer on x86_64
 * \author Alexander Boettcher
 * \date   2023-05-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SEL4__FUNCTIONS_X86_64_H_
#define _INCLUDE__SEL4__FUNCTIONS_X86_64_H_

LIBSEL4_INLINE_FUNC seL4_IPCBuffer *seL4_GetIPCBuffer(void)
{
	seL4_IPCBuffer * ptr = nullptr;
	asm volatile ("movq %%fs:0, %0" : "=r" (ptr) : :);
	return ptr;
}

#endif /* _INCLUDE__SEL4__FUNCTIONS_X86_64_H_ */
