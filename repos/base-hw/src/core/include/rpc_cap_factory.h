/*
 * \brief  RPC capability factory
 * \author Norman Feske
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__RPC_CAP_FACTORY_H_
#define _CORE__INCLUDE__RPC_CAP_FACTORY_H_

/* Genode includes */
#include <util/list.h>
#include <util/construct_at.h>
#include <base/lock.h>
#include <base/tslab.h>
#include <base/capability.h>
#include <base/log.h>

/* core-local includes */
#include <object.h>
#include <kernel/thread.h>

/* base-internal includes */
#include <base/internal/capability_space.h>

namespace Genode { class Rpc_cap_factory; }


class Genode::Rpc_cap_factory
{
	private:

		/**
		 * Kernel object placeholder held in a list
		 */
		struct Kobject : List<Kobject>::Element
		{
			using Identity = Kernel::Core_object_identity<Kernel::Thread>;

			Native_capability cap;

			uint8_t data[sizeof(Identity)]
			__attribute__((aligned(sizeof(addr_t))));
		};

		using Slab = Tslab<Kobject, get_page_size()>;

		uint8_t       _initial_slab_block[get_page_size()];
		Slab          _slab;
		List<Kobject> _list;
		Lock          _lock;

	public:

		Rpc_cap_factory(Allocator &md_alloc)
		:
			_slab(&md_alloc, &_initial_slab_block)
		{ }

		~Rpc_cap_factory()
		{
			Lock::Guard guard(_lock);

			while (Kobject * obj = _list.first()) {
				Kernel::delete_obj(obj->data);
				_list.remove(obj);
				destroy(&_slab, obj);
			}
		}

		/**
		 * Allocate RPC capability
		 *
		 * \throw Allocator::Out_of_memory
		 */
		Native_capability alloc(Native_capability ep)
		{
			Lock::Guard guard(_lock);

			/* allocate kernel object */
			Kobject * obj;
			if (!_slab.alloc(sizeof(Kobject), (void**)&obj))
				throw Allocator::Out_of_memory();
			construct_at<Kobject>(obj);

			/* create kernel object via syscall */
			Kernel::capid_t capid = Kernel::new_obj(obj->data, Capability_space::capid(ep));
			obj->cap = Capability_space::import(capid);
			if (!obj->cap.valid()) {
				raw("Invalid entrypoint ", (addr_t)Capability_space::capid(ep),
				    " for allocating a capability!");
				destroy(&_slab, obj);
				return Native_capability();
			}

			/* store it in the list and return result */
			_list.insert(obj);
			return obj->cap;
		}

		void free(Native_capability cap)
		{
			Lock::Guard guard(_lock);

			for (Kobject * obj = _list.first(); obj; obj = obj->next()) {
				if (obj->cap.data() == cap.data()) {
					Kernel::delete_obj(obj->data);
					_list.remove(obj);
					destroy(&_slab, obj);
					return;
				}
			}
		}
};

#endif /* _CORE__INCLUDE__RPC_CAP_FACTORY_H_ */
