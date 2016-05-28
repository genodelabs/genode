/*
 * \brief  Event tracing infrastructure
 * \author Norman Feske
 * \date   2013-08-09
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__TRACE__LOGGER_H_
#define _INCLUDE__BASE__TRACE__LOGGER_H_

#include <base/trace/buffer.h>
#include <cpu_session/cpu_session.h>

namespace Genode { namespace Trace {

	class Control;
	class Policy_module;
	class Logger;
} }


/**
 * Facility for logging events to a thread-specific buffer
 */
struct Genode::Trace::Logger
{
	private:

		Thread_capability  thread_cap;
		Cpu_session       *cpu;
		Control           *control;
		bool               enabled;
		unsigned           policy_version;
		Policy_module     *policy_module;
		Buffer            *buffer;
		size_t             max_event_size;

		bool               pending_init;

		bool _evaluate_control();

	public:

		Logger();

		bool initialized() { return control != 0; }

		bool init_pending() { return pending_init; }

		void init_pending(bool val) { pending_init = val; }

		void init(Thread_capability, Cpu_session*, Control*);

		/**
		 * Log binary data to trace buffer
		 */
		void log(char const *, size_t);

		/**
		 * Log event to trace buffer
		 */
		template <typename EVENT>
		void log(EVENT const *event)
		{
			if (!this || !_evaluate_control()) return;

			buffer->commit(event->generate(*policy_module, buffer->reserve(max_event_size)));
		}
};

#endif /* _INCLUDE__BASE__TRACE__LOGGER_H_ */
