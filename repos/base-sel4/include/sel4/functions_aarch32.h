/*
 * \brief  Implementation for seL4_GetIPCBuffer on aarch32
 * \author Alexander Boettcher
 * \date   2023-05-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SEL4__FUNCTIONS_AARCH32_H_
#define _INCLUDE__SEL4__FUNCTIONS_AARCH32_H_

LIBSEL4_INLINE_FUNC seL4_IPCBuffer *seL4_GetIPCBuffer(void)
{
	seL4_IPCBuffer ** ptr = nullptr;
	asm volatile ("mrc p15, 0, %0, c13, c0, 2" : "=r"(ptr));
	return *ptr;
}

#endif /* _INCLUDE__SEL4__FUNCTIONS_AARCH32_H_ */
