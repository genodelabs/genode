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
#include <internal/timer.h>
#include <internal/types.h>

namespace Libc { class Monitor; };


class Libc::Monitor : Interface
{
	public:

		struct Job;
		struct Pool;

	protected:

		struct Function : Interface { virtual bool execute() = 0; };

		virtual bool _monitor(Genode::Lock &, Function &, uint64_t) = 0;
		virtual void _charge_monitors() = 0;

	public:

		/**
		 * Block until monitored execution succeeds or timeout expires
		 *
		 * The mutex must be locked when calling the monitor. It is released
		 * during wait for completion and re-aquired before the function
		 * returns. This behavior is comparable to condition variables.
		 *
		 * Returns true if execution completed, false on timeout.
		 */
		template <typename FN>
		bool monitor(Genode::Lock &mutex, FN const &fn, uint64_t timeout_ms = 0)
		{
			struct _Function : Function
			{
				FN const &fn;
				bool execute() override { return fn(); }
				_Function(FN const &fn) : fn(fn) { }
			} function { fn };

			return _monitor(mutex, function, timeout_ms);
		}

		/**
		 * Charge monitor to execute the monitored function
		 */
		void charge_monitors() { _charge_monitors(); }
};


struct Libc::Monitor::Job : Timeout_handler
{
	private:

		Monitor::Function &_fn;

	protected:

		bool _completed { false };
		bool _expired   { false };

		Lock                   _blockade { Lock::LOCKED };
		Constructible<Timeout> _timeout;

	public:

		Job(Monitor::Function &fn,
		    Timer_accessor &timer_accessor, uint64_t timeout_ms)
		: _fn(fn)
		{
			if (timeout_ms) {
				_timeout.construct(timer_accessor, *this);
				_timeout->start(timeout_ms);
			}
		}

		bool completed() const { return _completed; }
		bool expired()   const { return _expired; }
		bool execute()         { return _fn.execute(); }

		virtual void wait_for_completion() { _blockade.lock(); }

		virtual void complete()
		{
			_completed = true;
			_blockade.unlock();
		}

		/**
		 * Timeout_handler interface
		 */
		void handle_timeout() override
		{
			_expired = true;
			_blockade.unlock();
		}
};


struct Libc::Monitor::Pool
{
	private:

		Registry<Job> _jobs;

		Lock _mutex;
		bool _execution_pending { false };

	public:

		void monitor(Genode::Lock &mutex, Job &job)
		{
			Registry<Job>::Element element { _jobs, job };

			mutex.unlock();

			job.wait_for_completion();

			mutex.lock();
		}

		bool charge_monitors()
		{
			Lock::Guard guard { _mutex };

			bool const charged = !_execution_pending;
			_execution_pending = true;
			return charged;
		}

		void execute_monitors()
		{
			{
				Lock::Guard guard { _mutex };

				if (!_execution_pending) return;

				_execution_pending = false;
			}

			_jobs.for_each([&] (Job &job) {
				if (!job.completed() && !job.expired() && job.execute()) {
					job.complete();
				}
			});
		}
};

#endif /* _LIBC__INTERNAL__MONITOR_H_ */
