/*
 * \brief  LOG output functions
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__LOG_H_
#define _INCLUDE__BASE__LOG_H_

#include <base/output.h>
#include <base/buffered_output.h>
#include <base/mutex.h>
#include <trace/timestamp.h>

namespace Genode {
	class Log;
	class Raw;
	class Trace_output;
	class Log_tsc_probe;
}


/**
 * Interface for writing output to the component's LOG session
 *
 * The LOG session is not used directly by the 'log', 'warning', and
 * 'error' functions. They access the LOG indirectly via this interface
 * instead, which ensures the proper synchronization of the output in the
 * presence of multiple threads and to apply distinguishable colors to the
 * different types of messages.
 */
class Genode::Log
{
	public:

		/**
		 * Type of message
		 */
		enum Type { LOG, WARNING, ERROR };

	private:

		Mutex _mutex { };

		Output &_output;

		void _acquire(Type);
		void _release();

	public:

		Log(Output &output) : _output(output) { }

		template <typename... ARGS>
		void output(Type type, ARGS &&... args)
		{
			/*
			 * This function is being inlined. Hence, we try to keep it as
			 * small as possible. For this reason, the mutex operations are
			 * performed by the '_acquire' and '_release' functions instead of
			 * using a mutex guard.
			 */
			_acquire(type);
			Output::out_args(_output, args...);
			_release();
		}

		/**
		 * Return component-global singleton instance of the 'Log'
		 */
		static Log &log();
};


/**
 * Raw-output back end
 *
 * \noapi
 */
class Genode::Raw
{
	private:

		static void    _acquire();
		static void    _release();
		static Output &_output();

	public:

		template <typename... ARGS>
		static void output(ARGS &&... args)
		{
			_acquire();
			Output::out_args(_output(), args...);
			_release();
		}
};


class Genode::Trace_output
{
	private:

		struct Write_trace_fn
		{
			void operator () (char const *);
		};

		/* cannot include log_session.h here because of recursion */
		enum { LOG_SESSION_MAX_STRING_LEN = 232 };
		typedef Buffered_output<LOG_SESSION_MAX_STRING_LEN,
		                        Write_trace_fn>
		        Buffered_trace_output;

	public:

		Trace_output() { }

		template <typename... ARGS>
		void output(ARGS &&... args)
		{
			Buffered_trace_output buffered_trace_output
			{ Write_trace_fn() };

			Output::out_args(buffered_trace_output, args...);
			buffered_trace_output.out_string("\n");
		}

		/**
		 * Return component-global singleton instance of the 'Trace_output'
		 */
		static Trace_output &trace_output();
};


namespace Genode {

	/**
	 * Write 'args' as a regular message to the log
	 */
	template <typename... ARGS>
	void log(ARGS &&... args) { Log::log().output(Log::LOG, args...); }


	/**
	 * Write 'args' as a warning message to the log
	 *
	 * The message is automatically prefixed with "Warning: ". Please refer to
	 * the description of the 'error' function regarding the convention of
	 * formatting error/warning messages.
	 */
	template <typename... ARGS>
	void warning(ARGS &&... args) { Log::log().output(Log::WARNING, args...); }


	/**
	 * Write 'args' as an error message to the log
	 *
	 * The message is automatically prefixed with "Error: ". Hence, the
	 * message argument does not need to additionally state that it is an error
	 * message. By convention, the actual message should be brief, starting
	 * with a lower-case character.
	 */
	template <typename... ARGS>
	void error(ARGS &&... args) { Log::log().output(Log::ERROR, args...); }


	/**
	 * Write 'args' directly via the kernel (i.e., kernel debugger)
	 *
	 * This function is intended for temporarily debugging purposes only.
	 */
	template <typename... ARGS>
	void raw(ARGS &&... args) { Raw::output(args...); }


	/**
	 * Write 'args' to the trace buffer if tracing is enabled
	 *
	 * The message is prefixed with a timestamp value
	 */
	template <typename... ARGS>
	void trace(ARGS && ... args) {
		Trace_output::trace_output().output(Trace::timestamp(), ": ", args...); }
}


/*
 * Helper for the 'GENODE_LOG_TSC' utility
 */
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

	public:

		class Stats : Noncopyable
		{
			private:

				friend class Log_tsc_probe;

				/* rate of log messages in number of calls */
				unsigned const _sample_rate;

				Timestamp _tsc_sum     = 0;  /* accumulated TSC ticks spent */
				unsigned  _calls       = 0;  /* number of executions */
				unsigned  _cycle_count = 0;  /* track sample rate */
				unsigned  _num_entered = 0;  /* recursion depth */
				Mutex     _mutex { };        /* protect stats */

				void enter()
				{
					Mutex::Guard guard(_mutex);

					_num_entered++;
				}

				void leave(char const * const name, Timestamp duration)
				{
					Mutex::Guard guard(_mutex);

					/*
					 * If the probed scope is executed recursively or
					 * concurrently by multiple threads, defer the accounting
					 * until the scope is completely left.
					 */
					_num_entered--;
					if (_num_entered > 0)
						return;

					_tsc_sum = _tsc_sum + duration;

					_calls++;
					_cycle_count++;

					if (_cycle_count < _sample_rate)
						return;

					log(" TSC ", name, ": ", Pretty_tsc { _tsc_sum }, " "
					    "(", _calls, " calls, last ", Pretty_tsc { duration }, ")");
					_cycle_count = 0;
				}

			public:

				Stats(unsigned sample_rate) : _sample_rate(sample_rate) { }
		};

	private:

		Stats &_stats;

		Timestamp const _start = Trace::timestamp();

		struct { char const *_name; };

	public:

		Log_tsc_probe(Stats &stats, char const *name)
		:
			_stats(stats), _name(name)
		{
			_stats.enter();
		}

		~Log_tsc_probe()
		{
			_stats.leave(_name, Trace::timestamp() - _start);
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
	static Genode::Log_tsc_probe::Stats genode_log_tsc_stats { n }; \
	Genode::Log_tsc_probe genode_log_tsc_probe(genode_log_tsc_stats, __func__);


/**
 * Variant of 'GENODE_LOG_TSC' that accepts the name of the probe as argument
 *
 * This variant is useful to disambiguate multiple scopes within one function
 * or when placing probes in methods that have the same name, e.g., overloads
 * within the same class or same-named methods of different classes.
 */
#define GENODE_LOG_TSC_NAMED(n, name) \
	static Genode::Log_tsc_probe::Stats genode_log_tsc_stats { n }; \
	Genode::Log_tsc_probe genode_log_tsc_probe(genode_log_tsc_stats, name);


#endif /* _INCLUDE__BASE__LOG_H_ */
