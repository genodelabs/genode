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
#include <base/allocator.h>
#include <base/attached_ram_dataspace.h>
#include <util/noncopyable.h>

/* core-local includes */
#include <dataspace_component.h>
#include <types.h>

namespace Core {
	template <typename T>
	class Phys_allocated;
}

template <typename T>
class Core::Phys_allocated : Genode::Noncopyable
{
	private:

		Rpc_entrypoint &_ep;
		Ram_allocator  &_ram;
		Local_rm       &_rm;

		Ram_allocator::Result _ram_result { _ram.try_alloc(sizeof(T)) };

		addr_t _phys_addr { 0 };

		Local_rm::Result _mapped {
			_ram_result.convert<Local_rm::Result>(
				[&] (Ram_allocator::Allocation const &ram) {
					_phys_addr = _ep.apply(ram.cap, [&](Dataspace_component *dsc) {
						return dsc->phys_addr(); });
					return _rm.attach(ram.cap, {
						.size       = { }, .offset    = { },
						.use_at     = { }, .at        = { },
						.executable = { }, .writeable = true });
				},
				[&] (Alloc_error) { return Local_rm::Error::INVALID_DATASPACE; }
			)};

	public:

		using Constructed = Attempt<Ok, Alloc_error>;

		Constructed const constructed = _mapped.convert<Constructed>(
			[&] (Local_rm::Attachment const &) { return Ok(); },
			[&] (Local_rm::Error) {
				return _ram_result.convert<Alloc_error>(
					[&] (Ram::Allocation const &) { return Alloc_error::DENIED; },
					[&] (Alloc_error e) { return e; }); });

		Phys_allocated(Rpc_entrypoint &ep,
		               Ram_allocator  &ram,
		               Local_rm       &rm)
		:
			_ep(ep), _ram(ram), _rm(rm)
		{
			obj([&] (T &o) { construct_at<T>(&o); });
		}

		Phys_allocated(Rpc_entrypoint &ep,
		               Ram_allocator  &ram,
		               Local_rm       &rm,
		               auto const     &construct_fn)
		:
			_ep(ep), _ram(ram), _rm(rm)
		{
			obj([&] (T &o) { construct_fn(*this, &o); });
		}

		~Phys_allocated()
		{
			obj([&] (T &o) { o.~T(); });
		}

		addr_t phys_addr() const { return _phys_addr; }

		void obj(auto const fn)
		{
			_mapped.with_result(
				[&] (Local_rm::Attachment const &a) { fn(*((T*)a.ptr)); },
				[&] (Local_rm::Error) { });
		}
};

#endif /* _CORE__PHYS_ALLOCATED_H_ */
