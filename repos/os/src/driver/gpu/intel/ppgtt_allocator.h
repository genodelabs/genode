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

			return _rm.attach(ds, {
				.size       = { },
				.offset     = { },
				.use_at     = { },
				.at         = { },
				.executable = { },
				.writeable  = true
			}).convert<Alloc_result>(

				[&] (Genode::Region_map::Range const range) -> Alloc_result {

					void * const va = (void*)range.start;
					void * const pa = (void*)_backend.dma_addr(ds);

					if (_map.add(ds, pa, va, range.num_bytes) == true) {
						if (_range.add_range(range.start, range.num_bytes).ok())
							return _range.alloc_aligned(size, 12);

						Genode::error("Ppgtt_allocator failed to extend meta data");
					}

					/* _map.add failed, roll back _rm.attach */
					_rm.detach(range.start);
					_backend.free(ds);
					return Alloc_error::DENIED;
				},

				[&] (Genode::Region_map::Attach_error e) {

					_backend.free(ds);

					using Error = Genode::Region_map::Attach_error;

					if (e == Error::OUT_OF_RAM)  return Alloc_error::OUT_OF_RAM;
					if (e == Error::OUT_OF_CAPS) return Alloc_error::OUT_OF_CAPS;
					return Alloc_error::DENIED;
				}
			);
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
