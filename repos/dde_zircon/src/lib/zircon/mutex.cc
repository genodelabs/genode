/*
 * \brief  Zircon mutex definitions
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/heap.h>
#include <base/allocator.h>
#include <base/mutex.h>

#include <threads.h>

#include <zx/static_resource.h>

extern "C" {

	int mtx_init(mtx_t *mtx, int type)
	{
		if (type & mtx_recursive){
			return thrd_error;
		}
		Genode::Allocator &alloc = ZX::Resource<Genode::Heap>::get_component();
		mtx->lock = static_cast<void *>(new (alloc) Genode::Mutex());
		return thrd_success;
	}

	int mtx_lock(mtx_t *mtx)
	{
		static_cast<Genode::Mutex *>(mtx->lock)->acquire();
		return thrd_success;
	}

	int mtx_unlock(mtx_t *mtx)
	{
		static_cast<Genode::Mutex *>(mtx->lock)->release();
		return thrd_success;
	}

}
