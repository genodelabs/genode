/*
 * \brief  Debug logging utilities
 * \author Norman Feske
 * \date   2021-04-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DEBUG__LOG_H_
#define _INCLUDE__DEBUG__LOG_H_

#include <base/log.h>
#include <base/mutex.h>
#include <trace/timestamp.h>

namespace Genode { template<char const *, unsigned> struct Log_tsc_probe; }


/*
 * Helper for the 'GENODE_LOG_TSC' utility
 *
 * The first template argument is solely used to prevent the linker from
 * merging multible instances of 'Log_tsc_probe::Stat' objects. The
 * 'GENODE_LOG_TSC' macro passes a dedicated static local array instance
 * as argument.
 */
template <char const *, unsigned SAMPLE_RATE>
class Genode::Log_tsc_probe : Noncopyable
{
	private:

		typedef Trace::Timestamp Timestamp;

		struct Pretty_tsc
		{
			Timestamp value;

			void print(Output &out) const
			{
				Timestamp const K = 1000, M = 1000*K, G = 1000*M;

				using Genode::print;

				if      (value > 100*G) print(out, value/G, "G");
				else if (value > 100*M) print(out, value/M, "M");
				else if (value > 100*K) print(out, value/K, "K");
				else                    print(out, value);
			}
		};

		struct Stats : Noncopyable
		{
			Timestamp _tsc_sum     = 0;  /* accumulated TSC ticks spent */
			unsigned  _calls       = 0;  /* number of executions */
			unsigned  _cycle_count = 0;  /* used to track sample rate */
			unsigned  _num_entered = 0;  /* recursion depth, or concurrency */
			Mutex     _mutex { };        /* protect stats */

			void enter()
			{
				Mutex::Guard guard(_mutex);

				_num_entered++;
				if (_num_entered > 1)
					return;
			}

			void leave(char const * const name, Timestamp duration)
			{
				Mutex::Guard guard(_mutex);

				/*
				 * If the probed scope is executed recursively or concurrently
				 * by multiple threads, defer the accounting until the scope
				 * is completely left.
				 */
				_num_entered--;
				if (_num_entered > 0)
					return;

				_tsc_sum = _tsc_sum + duration;

				_calls++;
				_cycle_count++;

				if (_cycle_count < SAMPLE_RATE)
					return;

				log("TSC ", name, ": ", Pretty_tsc { _tsc_sum }, " "
				    "(", _calls, " calls, last ", Pretty_tsc { duration }, ")");
				_cycle_count = 0;
			}
		};

		Stats &_singleton_stats()
		{
			static Stats stats;
			return stats;
		}

		Timestamp const _start = Trace::timestamp();

		struct { char const *_name; };

	public:

		Log_tsc_probe(char const *name) : _name(name)
		{
			_singleton_stats().enter();
		}

		~Log_tsc_probe()
		{
			_singleton_stats().leave(_name, Trace::timestamp() - _start);
		}
};


/**
 * Print TSC (time-stamp counter) ticks consumed by the calling function
 *
 * The macro captures the TSC ticks spent in the current scope and prints the
 * statistics about the accumulated cycles spent and the total number of calls.
 * For example,
 *
 *   TSC apply_config: 7072M (52 calls, last 314M)
 *
 * When this line appears in the log, the 'apply_config' function was executed
 * 52 times and consumed 7072 million TSC ticks. The last call took 314 million
 * ticks.
 *
 * The argument 'n' specifies the amount of calls after which the statistics
 * are printed. It allows for the tuning of the amount of output depending on
 * the instrumented function.
 */
#define GENODE_LOG_TSC(n) \
	static const char genode_log_tsc_dummy[] = "dummy"; \
	Genode::Log_tsc_probe<genode_log_tsc_dummy, n> genode_log_tsc_probe(__func__);


/**
 * Variant of 'GENODE_LOG_TSC' that accepts the name of the probe as argument
 *
 * This variant is useful to disambiguate multiple scopes within one function
 * or when placing probes in methods that have the same name, e.g., overloads
 * within the same class or same-named methods of different classes.
 */
#define GENODE_LOG_TSC_NAMED(n, name) \
	static const char genode_log_tsc_dummy[] = "dummy"; \
	Genode::Log_tsc_probe<genode_log_tsc_dummy, n> genode_log_tsc_probe(name);

#endif /* _INCLUDE__DEBUG__LOG_H_ */
