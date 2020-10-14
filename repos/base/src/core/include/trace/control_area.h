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
#include <base/attached_ram_dataspace.h>

/* base-internal includes */
#include <base/internal/trace_control.h>

namespace Genode { namespace Trace { class Control_area; } }


class Genode::Trace::Control_area
{
	public:

		enum { SIZE = 8192 };

	private:

		Ram_allocator                &_ram;
		Region_map                   &_rm;
		Attached_ram_dataspace const  _area;

		bool _index_valid(unsigned const index) const {
			return index < SIZE / sizeof(Trace::Control); }

		/*
		 * Noncopyable
		 */
		Control_area(Control_area const &);
		Control_area &operator = (Control_area const &);

		Trace::Control * _local_base() const {
			return _area.local_addr<Trace::Control>(); }

	public:

		Control_area(Ram_allocator &ram, Region_map &rm)
		:
			_ram(ram), _rm(rm), _area(ram, rm, SIZE)
		{ }

		~Control_area() { }

		Dataspace_capability dataspace() const { return _area.cap(); }

		bool alloc(unsigned &index_out)
		{
			for (unsigned index = 0; _index_valid(index); index++) {
				if (!_local_base()[index].is_free()) {
					continue;
				}

				_local_base()[index].alloc();
				index_out = index;
				return true;
			}

			error("trace-control allocation failed");
			return false;
		}

		void free(unsigned index)
		{
			if (_index_valid(index))
				_local_base()[index].reset();
		}

		Trace::Control *at(unsigned index)
		{
			return _index_valid(index) ? &(_local_base()[index]) : nullptr;
		}
};

#endif /* _CORE__INCLUDE__TRACE__CONTROL_AREA_H_ */
