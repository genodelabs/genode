/*
 * \brief  Capability allocation service
 * \author Stefan Kalkowski
 * \date   2015-03-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_

#include <base/allocator_guard.h>
#include <base/rpc_server.h>
#include <cap_session/cap_session.h>
#include <util/arg_string.h>
#include <util/construct_at.h>

#include <kernel/object.h>

namespace Genode { class Cap_session_component; }


class Genode::Cap_session_component : public Rpc_object<Cap_session>
{
	private:

		/**
		 * Kernel object placeholder hold in a list
		 */
		struct Kobject : List<Kobject>::Element
		{
			using Identity = Kernel::Core_object_identity<Kernel::Thread>;

			Native_capability cap;

			uint8_t data[sizeof(Identity)]
			__attribute__((aligned(sizeof(addr_t))));
		};

		using Slab = Tslab<Kobject, get_page_size()>;

		Allocator_guard _guard;
		uint8_t         _initial_sb[get_page_size()]; /* initial slab block */
		Slab            _slab;
		List<Kobject>   _list;
		Lock            _lock;

		/**
		 * Returns the right meta-data allocator,
		 * for core it returns a non-guarded one, otherwise a guard
		 */
		Allocator * _md_alloc(Allocator *md_alloc)
		{
			Allocator * core_mem_alloc =
				static_cast<Allocator*>(platform()->core_mem_alloc());
			return (md_alloc == core_mem_alloc) ? core_mem_alloc : &_guard;
		}

	public:

		Cap_session_component(Allocator *md_alloc, const char *args)
		: _guard(md_alloc,
		         Arg_string::find_arg(args, "ram_quota").long_value(0)),
		  _slab(_md_alloc(md_alloc), (Slab_block*)&_initial_sb) { }

		~Cap_session_component()
		{
			Lock::Guard guard(_lock);

			while (Kobject * obj = _list.first()) {
				Kernel::delete_obj(obj->data);
				_list.remove(obj);
				destroy(&_slab, obj);
			}
		}

		void upgrade_ram_quota(size_t ram_quota) { _guard.upgrade(ram_quota); }

		Native_capability alloc(Native_capability ep)
		{
			Lock::Guard guard(_lock);

			/* allocate kernel object */
			Kobject * obj;
			if (!_slab.alloc(sizeof(Kobject), (void**)&obj))
				throw Out_of_metadata();
			construct_at<Kobject>(obj);

			/* create kernel object via syscall */
			obj->cap = Kernel::new_obj(obj->data, ep.dst());
			if (!obj->cap.valid()) {
				PWRN("Invalid entrypoint %u for allocating a capability!",
				     ep.dst());
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

			for (Kobject * obj = _list.first(); obj; obj = obj->next())
				if (obj->cap.dst() == cap.dst()) {
					Kernel::delete_obj(obj->data);
					_list.remove(obj);
					destroy(&_slab, obj);
					return;
				}
		}
};

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
