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

#ifndef _CORE__INCLUDE__SIGNAL_BROKER_H_
#define _CORE__INCLUDE__SIGNAL_BROKER_H_

/* Genode includes */
#include <base/tslab.h>

/* core-local includes */
#include <assertion.h>
#include <signal_source_component.h>

namespace Genode { class Signal_broker; }


class Genode::Signal_broker
{
	private:

		template <typename T, size_t BLOCK_SIZE = get_page_size()>
		class Slab : public Tslab<T, BLOCK_SIZE>
		{
			private:

				uint8_t _first_block[BLOCK_SIZE];

			public:

				Slab(Allocator * const allocator)
				: Tslab<T, BLOCK_SIZE>(allocator, &_first_block) { }
		};

		Allocator                     &_md_alloc;
		Slab<Signal_source_component>  _sources_slab { &_md_alloc };
		Signal_source_pool             _sources;
		Slab<Signal_context_component> _contexts_slab { &_md_alloc };
		Signal_context_pool            _contexts;

	public:

		class Invalid_signal_source : public Exception { };

		Signal_broker(Allocator &md_alloc, Rpc_entrypoint &, Rpc_entrypoint &)
		:
			_md_alloc(md_alloc)
		{ }

		~Signal_broker()
		{
			_contexts.remove_all([this] (Signal_context_component *c) {
				destroy(&_contexts_slab, c);});
			_sources.remove_all([this] (Signal_source_component *s) {
				destroy(&_sources_slab, s);});
		}

		/*
		 * \throw Allocator::Out_of_memory
		 */
		Capability<Signal_source> alloc_signal_source()
		{
			/* the _sources_slab may throw Allocator::Out_of_memory */
			Signal_source_component *s = new (_sources_slab) Signal_source_component();
			_sources.insert(s);
			return reinterpret_cap_cast<Signal_source>(s->cap());
		}

		void free_signal_source(Capability<Signal_source> cap)
		{
			Signal_source_component *source = nullptr;
			auto lambda = [&] (Signal_source_component *s) {

				if (!s) {
					error("unknown signal source");
					return;
				}

				source = s;
				_sources.remove(source);
			};
			_sources.apply(cap, lambda);

			/* destruct signal source outside the lambda to prevent deadlock */
			if (source)
				destroy(&_sources_slab, source);
		}

		/*
		 * \throw Allocator::Out_of_memory
		 * \throw Invalid_signal_source
		 */
		Signal_context_capability
		alloc_context(Capability<Signal_source> source, unsigned long imprint)
		{
			auto lambda = [&] (Signal_source_component *s) {
				if (!s) {
					error("unknown signal source");
					throw Invalid_signal_source();
				}

				/* the _contexts_slab may throw Allocator::Out_of_memory */
				Signal_context_component *c = new (_contexts_slab)
					Signal_context_component(*s, imprint);

				_contexts.insert(c);
				return reinterpret_cap_cast<Signal_context>(c->cap());
			};
			return _sources.apply(source, lambda);
		}

		void free_context(Signal_context_capability context_cap)
		{
			Signal_context_component *context = nullptr;
			auto lambda = [&] (Signal_context_component *c) {

				if (!c) {
					error("unknown signal context");
					return;
				}

				context = c;
				_contexts.remove(context);
			};

			_contexts.apply(context_cap, lambda);

			/* destruct context outside the lambda to prevent deadlock */
			if (context)
				destroy(&_contexts_slab, context);
		}

		void submit(Signal_context_capability cap, unsigned cnt)
		{
			/*
			 * This function is never called as base-hw delivers signals
			 * directly via the kernel.
			 */
		}
};

#endif /* _CORE__INCLUDE__SIGNAL_BROKER_H_ */
