/*
 * \brief  Monitored execution in main context
 * \author Christian Helmuth
 * \author Christian Prochaska
 * \date   2020-01-09
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__MONITOR_H_
#define _LIBC__INTERNAL__MONITOR_H_

/* Genode includes */
#include <base/registry.h>

/* libc-internal includes */
#include <internal/types.h>

namespace Libc { class Blockade; };


class Libc::Blockade
{
	protected:

		bool _woken_up { false };
		bool _expired  { false };

	public:

		bool woken_up() const { return _woken_up; }
		bool expired()  const { return _expired; }

		virtual void block()  = 0;
		virtual void wakeup() = 0;
};


namespace Libc { class Monitor; };


class Libc::Monitor : Interface
{
	public:

		enum class Result { COMPLETE, TIMEOUT };

		struct Job;
		struct Pool;

		enum class Function_result { COMPLETE, INCOMPLETE };

		struct Function : Interface { virtual Function_result execute() = 0; };

	protected:

		virtual Result _monitor(Function &, uint64_t) = 0;
		virtual void _trigger_monitor_examination() = 0;

	public:

		/**
		 * Block until monitored execution completed or timeout expires
		 *
		 * Returns true if execution completed, false on timeout.
		 */
		template <typename FN>
		Result monitor(FN const &fn, uint64_t timeout_ms = 0)
		{
			struct _Function : Function
			{
				FN const &fn;
				Function_result execute() override { return fn(); }
				_Function(FN const &fn) : fn(fn) { }
			} function { fn };

			return _monitor(function, timeout_ms);
		}

		/**
		 * Trigger examination of monitored functions
		 */
		void trigger_monitor_examination() { _trigger_monitor_examination(); }
};


struct Libc::Monitor::Job
{
	private:

		Monitor::Function &_fn;
		Blockade          &_blockade;

	public:

		Job(Monitor::Function &fn, Blockade &blockade)
		: _fn(fn), _blockade(blockade) { }

		virtual ~Job() { }

		bool execute() { return _fn.execute() == Function_result::COMPLETE; }

		bool completed() const { return _blockade.woken_up(); }
		bool expired()   const { return _blockade.expired(); }

		void wait_for_completion() { _blockade.block(); }
		void complete()            { _blockade.wakeup(); }
};


struct Libc::Monitor::Pool
{
	private:

		Monitor       &_monitor;
		Registry<Job>  _jobs;

	public:

		Pool(Monitor &monitor) : _monitor(monitor) { }

		/* called by monitor-user context */
		void monitor(Job &job)
		{
			Registry<Job>::Element element { _jobs, job };

			_monitor.trigger_monitor_examination();

			job.wait_for_completion();
		}

		enum class State { JOBS_PENDING, ALL_COMPLETE };

		/* called by the monitor context itself */
		State execute_monitors()
		{
			State result = State::ALL_COMPLETE;

			_jobs.for_each([&] (Job &job) {

				if (!job.completed() && !job.expired()) {

					bool const completed = job.execute();

					if (completed)
						job.complete();

					if (!completed)
						result = State::JOBS_PENDING;
				}
			});

			return result;
		}
};

#endif /* _LIBC__INTERNAL__MONITOR_H_ */
