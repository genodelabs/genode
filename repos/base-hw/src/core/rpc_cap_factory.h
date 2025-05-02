/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__RPC_CAP_FACTORY_H_
#define _CORE__RPC_CAP_FACTORY_H_

/* Genode includes */
#include <util/list.h>
#include <util/construct_at.h>
#include <base/mutex.h>
#include <base/tslab.h>
#include <base/capability.h>

/* core includes */
#include <object.h>
#include <kernel/thread.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

namespace Core { class Rpc_cap_factory; }


class Core::Rpc_cap_factory
{
	private:

		/**
		 * Kernel object placeholder held in a list
		 */
		struct Kobject : List<Kobject>::Element
		{
			using O = Kernel::Core_object_identity<Kernel::Thread>;

			Constructible<O>  kobj {};
			Native_capability cap;

			Kobject(Native_capability ep)
			: cap(Capability_space::import(O::syscall_create(kobj, Capability_space::capid(ep)))) {}

			void destruct() { O::syscall_destroy(kobj); }
		};

		using Slab = Tslab<Kobject, get_page_size()>;

		uint8_t       _initial_slab_block[get_page_size()];
		Slab          _slab;
		List<Kobject> _list { };
		Mutex         _mutex { };

	public:

		Rpc_cap_factory(Allocator &md_alloc)
		:
			_slab(&md_alloc, &_initial_slab_block)
		{ }

		~Rpc_cap_factory()
		{
			Mutex::Guard guard(_mutex);

			while (Kobject * obj = _list.first()) {
				_list.remove(obj);
				destroy(&_slab, obj);
			}
		}

		using Alloc_result = Attempt<Native_capability, Alloc_error>;

		Alloc_result alloc(Native_capability ep)
		{
			Mutex::Guard guard(_mutex);

			return _slab.try_alloc(sizeof(Kobject)).convert<Alloc_result>(

				[&] (Memory::Allocation &a) -> Alloc_result {

					/* create kernel object */
					Kobject &obj = *construct_at<Kobject>(a.ptr, ep);

					if (!obj.cap.valid()) {
						raw("Invalid entrypoint ", (addr_t)Capability_space::capid(ep),
						    " for allocating a capability!");
						destroy(&_slab, &obj);
						return Alloc_error::DENIED;
					}

					/* store it in the list and return result */
					_list.insert(&obj);
					a.deallocate = false;
					return obj.cap;
				},
				[&] (Alloc_error e) { return e; });
		}

		void free(Native_capability cap)
		{
			Mutex::Guard guard(_mutex);

			for (Kobject * obj = _list.first(); obj; obj = obj->next()) {
				if (obj->cap.data() == cap.data()) {
					obj->destruct();
					_list.remove(obj);
					destroy(&_slab, obj);
					return;
				}
			}
		}
};

#endif /* _CORE__RPC_CAP_FACTORY_H_ */
