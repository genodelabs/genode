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


struct Genode::Trace::Control_area : Noncopyable
{
	enum { SIZE = 8192 };

	using Local_rm = Core::Local_rm;

	Ram_allocator::Result _ram;
	Local_rm     ::Result _mapped = Local_rm::Error::INVALID_DATASPACE;

	bool _index_valid(unsigned const index) const {
		return index < SIZE / sizeof(Control); }

	auto _with_control_at_index(unsigned i, auto const &fn,
	                                        auto const &missing_fn) const
	{
		if (!_index_valid(i)) {
			missing_fn();
			return;
		}

		return _mapped.with_result(
			[&] (Local_rm::Attachment const &a) { fn(((Control *)a.ptr)[i]); },
			[&] (Local_rm::Error)               { missing_fn(); });
	}

	using Constructed = Attempt<Ok, Alloc_error>;

	Constructed const constructed = _mapped.convert<Constructed>(
		[&] (Local_rm::Attachment const &) { return Ok(); },
		[&] (Local_rm::Error) {
			return _ram.convert<Alloc_error>(
				[&] (Ram::Allocation const &) { return Alloc_error::DENIED; /* never */ },
				[&] (Alloc_error e)     { return e; }); });

	Control_area(Ram_allocator &ram, Core::Local_rm &rm)
	:
		_ram(ram.try_alloc(SIZE)),
		_mapped(_ram.convert<Local_rm::Result>(
			[&] (Ram_allocator::Allocation const &ram) {
				return rm.attach(ram.cap, {
					.size       = { }, .offset    = { },
					.use_at     = { }, .at        = { },
					.executable = { }, .writeable = true });
			},
			[&] (Alloc_error) { return Local_rm::Error::INVALID_DATASPACE; }
		))
	{ }

	~Control_area() { }

	Ram::Capability dataspace() const
	{
		return _ram.convert<Ram::Capability>(
			[&] (Ram_allocator::Allocation const &ram) { return ram.cap; },
			[&] (Alloc_error) { return Ram::Capability(); });
	}

	struct Attr { unsigned index; };

	using Error  = Denied;
	using Slot   = Allocation<Control_area>;
	using Result = Slot::Attempt;

	Result alloc()
	{
		for (unsigned i = 0; _index_valid(i); i++) {

			bool ok = false;
			_with_control_at_index(i,
				[&] (Control &control) {
					if (control.is_free()) {
						control.alloc();
						ok = true; } },
				[&] { });

			if (ok)
				return { *this, { i } };
		}
		return Error();
	}

	void _free(Slot &slot)
	{
		_with_control_at_index(slot.index,
			[&] (Control &control) { control.reset(); return true; },
			[] { return true; });
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
