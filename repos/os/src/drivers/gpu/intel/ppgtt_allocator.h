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

		enum { ELEMENTS = 256, };
		Utils::Address_map<ELEMENTS> _map { };

	public:

		Ppgtt_allocator(Genode::Region_map      &rm,
		                Utils::Backend_alloc    &backend)
		:
			_rm         { rm },
			_backend    { backend }
		{ }

		/*************************
		 ** Allocator interface **
		 *************************/

		Alloc_result try_alloc(size_t size) override
		{
			Genode::Ram_dataspace_capability ds { };

			try {
				ds = _backend.alloc(size, _caps_guard, _ram_guard);
			}
			catch (Genode::Out_of_ram)  { return Alloc_error::OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { return Alloc_error::OUT_OF_CAPS; }
			catch (...)                 { return Alloc_error::DENIED; }

			Alloc_error alloc_error = Alloc_error::DENIED;

			try {
				void * const ptr = _rm.attach(ds);

				if (_map.add(ds, ptr))
					return ptr;

				/* _map.add failed, roll back _rm.attach */
				_rm.detach(ptr);
			}
			catch (Genode::Out_of_ram)  { alloc_error = Alloc_error::OUT_OF_RAM;  }
			catch (Genode::Out_of_caps) { alloc_error = Alloc_error::OUT_OF_CAPS; }
			catch (...)                 { alloc_error = Alloc_error::DENIED; }

			/* roll back allocation */
			_backend.free(ds);

			return alloc_error;
		}

		void free(void *addr, size_t) override
		{
			if (addr == nullptr) { return; }

			Genode::Ram_dataspace_capability cap = _map.remove(addr);
			if (!cap.valid()) {
				Genode::error("could not lookup capability for addr: ", addr);
				return;
			}

			_rm.detach(addr);
			_backend.free(cap);
		}

		bool   need_size_for_free() const override { return false; }
		size_t overhead(size_t)     const override { return 0; }

		/*******************************************
		 ** Translation_table_allocator interface **
		 *******************************************/

		void *phys_addr(void *va) override
		{
			if (va == nullptr) { return nullptr; }
			typename Utils::Address_map<ELEMENTS>::Element *e = _map.phys_addr(va);
			return e ? (void*)e->pa : nullptr;
		}

		void *virt_addr(void *pa) override
		{
			if (pa == nullptr) { return nullptr; }
			typename Utils::Address_map<ELEMENTS>::Element *e = _map.virt_addr(pa);
			return e ? (void*)e->va : nullptr;
		}
};

#endif /* _PPGTT_ALLOCATOR_H_ */
