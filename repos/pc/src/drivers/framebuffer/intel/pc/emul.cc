/*
 * \brief  Linux emulation backend functions
 * \author Josef Soentgen
 * \date   2021-03-22
 */

/*
 * Copyright (C) 2021-2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* lx emulation includes */
#include <lx_kit/env.h>

/* local includes */
#include "lx_emul.h"


void *emul_alloc_shmem_file_buffer(unsigned long size)
{
	auto &buffer = Lx_kit::env().memory.alloc_buffer(size);
	return reinterpret_cast<void *>(buffer.virt_addr());
}


void emul_free_shmem_file_buffer(void *addr)
{
	Lx_kit::env().memory.free_buffer(addr);
}
