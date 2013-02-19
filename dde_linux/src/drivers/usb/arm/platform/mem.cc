/*
 * \brief  Backend for allocating DMA memory on ARM
 * \author Alexander Boettcher
 * \date   2013-02-19
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include "mem.h"

Genode::Ram_dataspace_capability Genode::Mem::alloc_dma_buffer(size_t size) {
	return Genode::env()->ram_session()->alloc(size, false); }
