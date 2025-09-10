/*
 * \brief  Allocate an object with a physical address
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2024-12-02
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__PHYS_ALLOCATED_H_
#define _CORE__PHYS_ALLOCATED_H_

/* base includes */
#include <util/noncopyable.h>

/* core-local includes */
#include <core_ram.h>
#include <types.h>

namespace Core {
	template <typename T>
	class Phys_allocated;
}

template <typename T>
class Core::Phys_allocated : Genode::Noncopyable
{
	private:

		Ram_obj_allocator<T>         _ram;
		Ram_obj_allocator<T>::Result _result;

		using Allocation = Ram_obj_allocator<T>::Allocation;
		using Error      = Ram_obj_allocator<T>::Error;

	public:

		using Constructed = Attempt<Ok, Error>;

		Constructed const constructed = _result.template convert<Constructed>(
			[&] (Allocation const &) { return Ok(); },
			[&] (Error e)            { return e;    });

		Phys_allocated(Accounted_mapped_ram_allocator &ram, auto &&... args)
		: _ram(ram), _result(_ram.create(args...)) { }

		addr_t phys_addr() const
		{
			return _result.template convert<addr_t>(
				[&] (Allocation const &a) { return a._attr.phys; },
				[&] (Error)               { return 0UL; });
		}

		void obj(auto const fn)
		{
			_result.template with_result(
				[&] (Allocation const &a) { fn(a.obj); },
				[&] (Error)               { });
		}
};

#endif /* _CORE__PHYS_ALLOCATED_H_ */
