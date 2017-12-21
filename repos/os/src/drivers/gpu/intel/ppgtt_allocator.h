/*
 * \brief  PPGTT translation table allocator
 * \author Josef Soentgen
 * \data   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PPGTT_ALLOCATOR_H_
#define _PPGTT_ALLOCATOR_H_

/* Genode includes */
#include <base/allocator_guard.h>

/* local includes */
#include <types.h>
#include <utils.h>


namespace Igd {

	class Ppgtt_allocator;
}


class Igd::Ppgtt_allocator : public Genode::Translation_table_allocator
{
	private:

		Genode::Region_map      &_rm;
		Genode::Allocator_guard &_guard;
		Utils::Backend_alloc    &_backend;

		enum { ELEMENTS = 256, };
		Utils::Address_map<ELEMENTS> _map { };

	public:

		Ppgtt_allocator(Genode::Region_map      &rm,
		                Genode::Allocator_guard &guard,
		                Utils::Backend_alloc    &backend)
		: _rm(rm), _guard(guard), _backend(backend) { }

		/*************************
		 ** Allocator interface **
		 *************************/

		bool alloc(size_t size, void **out_addr)
		{
			Genode::Ram_dataspace_capability ds = _backend.alloc(_guard, size);
			if (!ds.valid()) { return false; }

			*out_addr = _rm.attach(ds);
			return _map.add(ds, *out_addr);
		}

		void free(void *addr, size_t)
		{
			if (addr == nullptr) { return; }

			Genode::Ram_dataspace_capability cap = _map.remove(addr);
			if (!cap.valid()) {
				Genode::error("could not lookup capability for addr: ", addr);
				return;
			}

			_rm.detach(addr);
			_backend.free(_guard, cap);
		}

		bool   need_size_for_free() const override { return false; }
		size_t overhead(size_t)     const override { return 0; }

		/*******************************************
		 ** Translation_table_allocator interface **
		 *******************************************/

		void *phys_addr(void *va)
		{
			if (va == nullptr) { return nullptr; }
			typename Utils::Address_map<ELEMENTS>::Element *e = _map.phys_addr(va);
			return e ? (void*)e->pa : nullptr;
		}

		void *virt_addr(void *pa)
		{
			if (pa == nullptr) { return nullptr; }
			typename Utils::Address_map<ELEMENTS>::Element *e = _map.virt_addr(pa);
			return e ? (void*)e->va : nullptr;
		}
};

#endif /* _PPGTT_ALLOCATOR_H_ */
