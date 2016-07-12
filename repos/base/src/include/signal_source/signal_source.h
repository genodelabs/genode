/*
 * \brief  Signal-source interface
 * \author Norman Feske
 * \date   2010-02-03
 *
 * This file is only included by 'pd_session/pd_session.h' and relies
 * on the headers included there. It is a separate header file to make it
 * easily replaceable by a platform-specific implementation.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__SIGNAL_SOURCE_H_
#define _INCLUDE__SIGNAL_SOURCE__SIGNAL_SOURCE_H_

namespace Genode { class Signal_source; }

/**
 * Blocking part of the PD-session interface
 *
 * The blocking 'wait_for_signal()' operation cannot be part of the
 * PD-session interface because otherwise, context allocations or
 * signal submissions would not be possible while blocking for signals.
 * Therefore, the blocking part is implemented as a kernel-specific
 * special case.
 */
struct Genode::Signal_source
{
	class Signal
	{
		private:

			long _imprint;
			int  _num;

		public:

			Signal(long imprint, int num) : _imprint(imprint), _num(num) { }

			Signal() : _imprint(0), _num(0) { }

			long imprint() { return _imprint; }

			int num() { return _num; }
	};

	virtual ~Signal_source() { }

	/**
	 * Wait for signal
	 */
	virtual Signal wait_for_signal() = 0;


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_wait_for_signal, Signal, wait_for_signal);
	GENODE_RPC_INTERFACE(Rpc_wait_for_signal);
};

#endif /* _INCLUDE__SIGNAL_SOURCE__SIGNAL_SOURCE_H_ */
