/*
 * \brief  Trace control area
 * \author Norman Feske
 * \date   2013-08-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRACE__CONTROL_AREA_H_
#define _CORE__INCLUDE__TRACE__CONTROL_AREA_H_

#include <base/env.h>
#include <dataspace/capability.h>

/* base-internal includes */
#include <base/internal/trace_control.h>

namespace Genode { namespace Trace { class Control_area; } }


class Genode::Trace::Control_area
{
	public:

		enum { SIZE = 8192 };

	private:

		Ram_allocator           &_ram;
		Region_map              &_rm;
		Ram_dataspace_capability _ds;
		Trace::Control          *_local_base;

		static Ram_dataspace_capability _try_alloc(Ram_allocator &ram, size_t size)
		{
			try { return ram.alloc(size); }
			catch (...) { return Ram_dataspace_capability(); }
		}

		static Trace::Control *_try_attach(Region_map &rm, Dataspace_capability ds)
		{
			try { return rm.attach(ds); }
			catch (...) { return nullptr; }
		}

		bool _index_valid(int index) const
		{
			return (index + 1)*sizeof(Trace::Control) < SIZE;
		}

		/*
		 * Noncopyable
		 */
		Control_area(Control_area const &);
		Control_area &operator = (Control_area const &);

	public:

		Control_area(Ram_allocator &ram, Region_map &rm)
		:
			_ram(ram), _rm(rm),
			_ds(_try_alloc(ram, SIZE)),
			_local_base(_try_attach(rm, _ds))
		{ }

		~Control_area()
		{
			if (_local_base) _rm.detach(_local_base);
			if (_ds.valid()) _ram.free(_ds);
		}

		Dataspace_capability dataspace() const { return _ds; }

		bool alloc(unsigned &index_out)
		{
			for (unsigned index = 0; _index_valid(index); index++) {
				if (!_local_base[index].is_free()) {
					continue;
				}

				_local_base[index].alloc();
				index_out = index;
				return true;
			}

			error("trace-control allocaton failed");
			return false;
		}

		void free(unsigned index)
		{
			if (_index_valid(index))
				_local_base[index].reset();
		}

		Trace::Control *at(unsigned index)
		{
			return _index_valid(index) ? &_local_base[index] : nullptr;
		}
};

#endif /* _CORE__INCLUDE__TRACE__CONTROL_AREA_H_ */
