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


struct Genode::Trace::Control_area : Noncopyable
{
	enum { SIZE = 8192 };

	Alloc_error error = Alloc_error::DENIED;
	Constructible<Attached_ram_dataspace> _area { };

	bool _index_valid(unsigned const index) const {
		return index < SIZE / sizeof(Control); }

	auto _with_control_at_index(unsigned i, auto const &fn,
	                                        auto const &missing_fn) const
	-> decltype(missing_fn())
	{
		if (!_area.constructed() || !_index_valid(i))
			return missing_fn();

		return fn(_area->local_addr<Control>()[i]);
	}

	Control_area(Ram_allocator &ram, Region_map &rm)
	{
		try { _area.construct(ram, rm, SIZE); }
		catch (Out_of_ram)  { error = Alloc_error::OUT_OF_RAM; }
		catch (Out_of_caps) { error = Alloc_error::OUT_OF_CAPS; }
		catch (...)         { }
	}

	~Control_area() { }

	bool valid() const { return _area.constructed(); }

	Ram::Capability dataspace() const
	{
		return _area.constructed() ? _area->cap() : Ram::Capability();
	}

	struct Attr { unsigned index; };

	using Error  = Denied;
	using Slot   = Allocation<Control_area>;
	using Result = Slot::Attempt;

	Result alloc()
	{
		auto try_alloc = [&] (Control &control)
		{
			if (!control.is_free())
				return false;
			control.alloc();
			return true;
		};

		for (unsigned i = 0; _index_valid(i); i++)
			if (_with_control_at_index(i, try_alloc, [&] { return false; }))
				return { *this, { i } };

		return Error();
	}

	void _free(Slot &slot)
	{
		_with_control_at_index(slot.index,
			[&] (Control &control) { control.reset(); }, [] { });
	}

	void with_control(Result const &slot, auto const &fn) const
	{
		slot.with_result(
			[&] (Slot const &slot) {
				_with_control_at_index(slot.index, fn, [&] { }); },
			[&] (Error) { });
	}
};

#endif /* _CORE__INCLUDE__TRACE__CONTROL_AREA_H_ */
