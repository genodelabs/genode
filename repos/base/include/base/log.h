/*
 * \brief  LOG output functions
 * \author Norman Feske
 * \date   2016-05-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__LOG_H_
#define _INCLUDE__BASE__LOG_H_

#include <base/output.h>
#include <base/lock.h>

namespace Genode { class Log; }


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

		Lock    _lock;
		void    _acquire(Type);
		void    _release();
		Output &_output;

		/**
		 * Helper for the sequential output of a variable list of arguments
		 */
		template <typename HEAD, typename... TAIL>
		void _output_args(Output &output, HEAD && head, TAIL &&... tail)
		{
			print(output, head);
			_output_args(output, tail...);
		}

		template <typename LAST>
		void _output_args(Output &output, LAST && last) { print(output, last); }

	public:

		Log(Output &output) : _output(output) { }

		template <typename... ARGS>
		void output(Type type, ARGS &&... args)
		{
			/*
			 * This function is being inlined. Hence, we try to keep it as
			 * small as possible. For this reason, the lock operations are
			 * performed by the '_acquire' and '_release' functions instead of
			 * using a lock guard.
			 */
			_acquire(type);
			_output_args(_output, args...);
			_release();
		}

		/**
		 * Return component-global singleton instance of the 'Log'
		 */
		static Log &log();
};


namespace Genode {

	/**
	 * Write 'args' as a regular message to the log
	 */
	template <typename... ARGS>
	void log(ARGS &&... args) { Log::log().output(Log::LOG, args...); }


	/**
	 * Write 'args' as a warning message to the log
	 */
	template <typename... ARGS>
	void warning(ARGS &&... args) { Log::log().output(Log::WARNING, args...); }


	/**
	 * Write 'args' as an error message to the log
	 */
	template <typename... ARGS>
	void error(ARGS &&... args) { Log::log().output(Log::ERROR, args...); }
}

#endif /* _INCLUDE__BASE__LOG_H_ */
