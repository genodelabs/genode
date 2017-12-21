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
#include <base/lock.h>

namespace Genode {

	class Log;
	class Raw;
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

		Lock _lock { };

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
			 * small as possible. For this reason, the lock operations are
			 * performed by the '_acquire' and '_release' functions instead of
			 * using a lock guard.
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
}

#endif /* _INCLUDE__BASE__LOG_H_ */
