/*
 * \brief  Base-hw-specific signal-delivery mechanism
 * \author Norman Feske
 * \date   2016-01-04
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SIGNAL_BROKER_H_
#define _CORE__SIGNAL_BROKER_H_

/* Genode includes */
#include <base/tslab.h>

/* core includes */
#include <signal_source_component.h>

namespace Core { class Signal_broker; }


class Core::Signal_broker
{
	private:

		template <typename T, size_t BLOCK_SIZE = PAGE_SIZE>
		class Slab : public Tslab<T, BLOCK_SIZE>
		{
			private:

				uint8_t _first_block[BLOCK_SIZE];

			public:

				Slab(Allocator * const allocator)
				: Tslab<T, BLOCK_SIZE>(allocator, &_first_block) { }
		};

		Allocator                     &_md_alloc;
		Slab<Signal_source_component>  _source_slab { &_md_alloc };
		Signal_source_pool             _sources { };
		Slab<Signal_context_component> _context_slab { &_md_alloc };
		Signal_context_pool            _contexts { };

		using Context_alloc = Memory::Constrained_obj_allocator<Signal_context_component>;
		using Source_alloc  = Memory::Constrained_obj_allocator<Signal_source_component>;

		Context_alloc _context_alloc { _context_slab };
		Source_alloc  _source_alloc  { _source_slab  };

	public:

		Signal_broker(Allocator &md_alloc, Rpc_entrypoint &, Rpc_entrypoint &)
		:
			_md_alloc(md_alloc)
		{ }

		~Signal_broker()
		{
			_contexts.remove_all([this] (Signal_context_component *c) {
				platform_specific().revoke.revoke_signal_context(reinterpret_cap_cast<Signal_context>(c->cap()));
				_context_alloc.destroy(*c); });

			_sources.remove_all([this] (Signal_source_component *s) {
				_source_alloc.destroy(*s); });
		}

		using Alloc_source_result = Attempt<Capability<Signal_source>, Alloc_error>;

		Alloc_source_result alloc_signal_source()
		{
			return _source_alloc.create().convert<Alloc_source_result>(
				[&] (Source_alloc::Allocation &a) {
					_sources.insert(&a.obj);
					a.deallocate = false;
					return reinterpret_cap_cast<Signal_source>(a.obj.cap());
				},
				[&] (Alloc_error e) { return e; }
			);
		}

		void free_signal_source(Capability<Signal_source> cap)
		{
			Signal_source_component *source_ptr = nullptr;
			_sources.apply(cap, [&] (Signal_source_component *s) {
				source_ptr = s; });

			if (!source_ptr)
				return;

			/* destruct signal source outside 'apply' to prevent deadlock */
			_sources.remove(source_ptr);
			_source_alloc.destroy(*source_ptr);
		}

		using Alloc_context_result = Attempt<Signal_context_capability, Alloc_error>;

		Alloc_context_result alloc_context(Capability<Signal_source> source, addr_t imprint)
		{
			return _sources.apply(source,
				[&] (Signal_source_component *s) -> Alloc_context_result {
					if (!s) return Alloc_error::DENIED;

					return _context_alloc.create(*s, imprint).convert<Alloc_context_result>(
						[&] (Context_alloc::Allocation &a) {
							_contexts.insert(&a.obj);
							a.deallocate = false;
							return reinterpret_cap_cast<Signal_context>(a.obj.cap());
						},
						[&] (Alloc_error e) { return e; });
			});
		}

		void free_context(Signal_context_capability context_cap)
		{
			platform_specific().revoke.revoke_signal_context(context_cap);

			Signal_context_component *context_ptr = nullptr;
			_contexts.apply(context_cap, [&] (Signal_context_component *c) {
				context_ptr = c; });

			if (!context_ptr)
				return;

			/* destruct context outside the lambda to prevent deadlock */
			_contexts.remove(context_ptr);
			_context_alloc.destroy(*context_ptr);
		}

		void submit(Signal_context_capability, unsigned)
		{
			/*
			 * This function is never called as base-hw delivers signals
			 * directly via the kernel.
			 */
		}
};

#endif /* _CORE__SIGNAL_BROKER_H_ */
