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
		Utils::Backend_alloc    &_backend;

		enum { ELEMENTS = 128, }; /* max 128M for page tables */
		Utils::Address_map<ELEMENTS> _map { };

		Genode::Allocator_avl    _range;

	public:

		Ppgtt_allocator(Genode::Allocator       &md_alloc,
		                Genode::Region_map      &rm,
		                Utils::Backend_alloc    &backend)
		:
			_rm         { rm },
			_backend    { backend },
			_range      { &md_alloc }
		{ }

		~Ppgtt_allocator()
		{
			_map.for_each([&](Utils::Address_map<ELEMENTS>::Element &elem) {
				_rm.detach(elem.va);
				_backend.free(elem.ds_cap);
				elem.invalidate();
			});
		}

		/*************************
		 ** Allocator interface **
		 *************************/

		Alloc_result try_alloc(size_t size) override
		{
			Alloc_result result = _range.alloc_aligned(size, 12);
			if (result.ok()) return result;

			Genode::Ram_dataspace_capability ds { };

			size_t alloc_size = 1024*1024;

			try {
				ds = _backend.alloc(alloc_size);
			}
			catch (Gpu::Session::Out_of_ram)  { throw; }
			catch (Gpu::Session::Out_of_caps) { throw; }
			catch (...) { return Alloc_error::DENIED; }

			Alloc_error alloc_error = Alloc_error::DENIED;

			try {
				void * const va = _rm.attach(ds);
				void * const pa = (void*)_backend.dma_addr(ds);

				if (_map.add(ds, pa, va, alloc_size) == true) {
					_range.add_range((Genode::addr_t)va, alloc_size);
					result = _range.alloc_aligned(size, 12);
					return result;
				}

				/* _map.add failed, roll back _rm.attach */
				_rm.detach(va);
			}
			catch (Genode::Out_of_ram)  { alloc_error = Alloc_error::OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { alloc_error = Alloc_error::OUT_OF_CAPS; }
			catch (...)                 { alloc_error = Alloc_error::DENIED; }

			/* roll back allocation */
			_backend.free(ds);
			return alloc_error;
		}

		void free(void *addr, size_t size) override
		{
			if (addr == nullptr) { return; }

			_range.free(addr, size);
		}

		bool   need_size_for_free() const override { return false; }
		size_t overhead(size_t)     const override { return 0; }

		/*******************************************
		 ** Translation_table_allocator interface **
		 *******************************************/

		void *phys_addr(void *va) override
		{
			if (va == nullptr) { return nullptr; }
			addr_t pa = _map.phys_addr(va);
			return pa ? (void *)pa : nullptr;
		}

		void *virt_addr(void *pa) override
		{
			if (pa == nullptr) { return nullptr; }
			addr_t virt = _map.virt_addr(pa);
			return virt ? (void*)virt : nullptr;
		}
};

#endif /* _PPGTT_ALLOCATOR_H_ */
