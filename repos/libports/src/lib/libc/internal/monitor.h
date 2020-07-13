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

		virtual Result _monitor(Mutex &, Function &, uint64_t) = 0;
		virtual void _charge_monitors() = 0;

	public:

		/**
		 * Block until monitored execution completed or timeout expires
		 *
		 * The mutex must be locked when calling the monitor. It is released
		 * during wait for completion and re-aquired before the function
		 * returns. This behavior is comparable to condition variables.
		 *
		 * Returns true if execution completed, false on timeout.
		 */
		template <typename FN>
		Result monitor(Mutex &mutex, FN const &fn, uint64_t timeout_ms = 0)
		{
			struct _Function : Function
			{
				FN const &fn;
				Function_result execute() override { return fn(); }
				_Function(FN const &fn) : fn(fn) { }
			} function { fn };

			_charge_monitors();
			return _monitor(mutex, function, timeout_ms);
		}

		/**
		 * Charge monitor to execute the monitored function
		 */
		void charge_monitors() { _charge_monitors(); }
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
		void monitor(Mutex &mutex, Job &job)
		{
			Registry<Job>::Element element { _jobs, job };

			mutex.release();

			_monitor.charge_monitors();

			job.wait_for_completion();

			mutex.acquire();
		}

		/* called by the monitor context itself */
		void execute_monitors()
		{
			_jobs.for_each([&] (Job &job) {
				if (!job.completed() && !job.expired() && job.execute()) {
					job.complete();
				}
			});
		}
};

#endif /* _LIBC__INTERNAL__MONITOR_H_ */
