/*
 * \brief  Signal-delivery mechanism
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SIGNAL_SOURCE_COMPONENT_H_
#define _CORE__INCLUDE__SIGNAL_SOURCE_COMPONENT_H_

#include <signal_source/rpc_object.h>
#include <base/allocator_guard.h>
#include <base/tslab.h>
#include <base/lock.h>
#include <base/rpc_client.h>
#include <base/rpc_server.h>
#include <util/fifo.h>
#include <base/signal.h>

namespace Genode {

	class Signal_context_component;
	class Signal_source_component;

	typedef Fifo<Signal_context_component> Signal_queue;

	struct Signal_context_slab;
}


class Genode::Signal_context_component : public Rpc_object<Signal_context>,
                                         public Signal_queue::Element
{
	private:

		long                     _imprint;
		int                      _cnt;
		Signal_source_component *_source;

	public:

		/**
		 * Constructor
		 */
		Signal_context_component(long imprint,
		                         Signal_source_component *source)
		: _imprint(imprint), _cnt(0), _source(source) { }

		/**
		 * Destructor
		 */
		inline ~Signal_context_component();

		/**
		 * Increment number of signals to be delivered at once
		 */
		void increment_signal_cnt(int increment) { _cnt += increment; }

		/**
		 * Reset number of pending signals
		 */
		void reset_signal_cnt() { _cnt = 0; }

		long                          imprint()  { return _imprint; }
		int                           cnt()      { return _cnt; }
		Signal_source_component      *source()   { return _source; }
};


class Genode::Signal_source_component : public Signal_source_rpc_object
{
	private:

		Signal_queue          _signal_queue;
		Rpc_entrypoint       *_entrypoint;
		Native_capability     _reply_cap;

	public:

		/**
		 * Constructor
		 */
		Signal_source_component(Rpc_entrypoint *rpc_entrypoint);

		~Signal_source_component();

		void release(Signal_context_component *context);

		void submit(Signal_context_component *context,
		            unsigned long             cnt);

		/*****************************
		 ** Signal-receiver interface **
		 *****************************/

		Signal wait_for_signal() override;
};


Genode::Signal_context_component::~Signal_context_component()
{
	if (enqueued() && _source)
		_source->release(this);
}

#endif /* _CORE__INCLUDE__SIGNAL_SOURCE_COMPONENT_H_ */
