/*
 * \brief  Mechanism to deliver signals via core
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

/* core includes */
#include <signal_source_component.h>
#include <signal_context_slab.h>
#include <signal_delivery_proxy.h>

namespace Core { class Signal_broker; }


class Core::Signal_broker
{
	private:

		Allocator                &_md_alloc;
		Rpc_entrypoint           &_source_ep;
		Rpc_entrypoint           &_context_ep;
		Signal_source_component   _source;
		Capability<Signal_source> _source_cap;
		Signal_context_slab       _context_slab { _md_alloc };
		Signal_delivery_proxy_component _delivery_proxy { _source_ep };

		using Context_alloc = Memory::Constrained_obj_allocator<Signal_context_component>;

		Context_alloc _context_alloc { _context_slab };

	public:

		Signal_broker(Allocator      &md_alloc,
		              Rpc_entrypoint &source_ep,
		              Rpc_entrypoint &context_ep)
		:
			_md_alloc(md_alloc),
			_source_ep(source_ep),
			_context_ep(context_ep),
			_source(_context_ep),
			_source_cap(_source_ep.manage(&_source))
		{ }

		~Signal_broker()
		{
			/* remove source from entrypoint */
			_source_ep.dissolve(&_source);

			/* free all signal contexts */
			while (Signal_context_component *r = _context_slab.any_signal_context())
				free_context(r->cap());
		}

		using Alloc_source_result = Attempt<Capability<Signal_source>, Alloc_error>;

		Alloc_source_result alloc_signal_source() { return _source_cap; }

		void free_signal_source(Capability<Signal_source>) { }

		using Alloc_context_result = Attempt<Signal_context_capability, Alloc_error>;

		Alloc_context_result alloc_context(Capability<Signal_source>, unsigned long imprint)
		{
			/*
			 * Ignore the signal-source argument as we create only a single
			 * receiver for each PD.
			 */
			return _context_alloc.create(imprint, _source).convert<Alloc_context_result>(
				[&] (Context_alloc::Allocation &a) {
					a.deallocate = false;
					return _context_ep.manage(&a.obj);
				},
				[&] (Alloc_error e) { return e; });
		}

		void free_context(Signal_context_capability context_cap)
		{
			Signal_context_component *context = nullptr;

			_context_ep.apply(context_cap, [&] (Signal_context_component *c) {

				if (!c) {
					warning("specified signal-context capability has wrong type");
					return;
				}

				context = c;

				_context_ep.dissolve(context);
			});

			if (!context)
				return;

			/* release solely in context of context_ep thread */
			if (context->enqueued() && !_context_ep.is_myself())
					_delivery_proxy.release(*context);

			_context_alloc.destroy(*context);
		}

		void submit(Signal_context_capability const cap, unsigned const cnt)
		{
			_delivery_proxy.submit(cap, cnt);
		}
};

#endif /* _CORE__INCLUDE__SIGNAL_BROKER_H_ */
