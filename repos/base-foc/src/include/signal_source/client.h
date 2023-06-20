/*
 * \brief  Fiasco.OC-specific signal-source client interface
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2010-02-03
 *
 * On Fiasco.OC, the signal source server does not provide a blocking
 * 'wait_for_signal' function because this kernel does not support out-of-order
 * IPC replies. Instead, we use an IRQ kernel-object to let the client block
 * until a signal is present at the server.
 *
 * We request the IRQ object capability and attach to the IRQ on construction
 * of the 'Signal_source_client' object.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__CLIENT_H_
#define _INCLUDE__SIGNAL_SOURCE__CLIENT_H_

#include <base/rpc_client.h>
#include <base/thread.h>
#include <cpu_session/cpu_session.h>
#include <signal_source/foc_signal_source.h>

namespace Genode { class Signal_source_client; }


class Genode::Signal_source_client : public Rpc_client<Foc_signal_source>
{
	private:

		/**
		 * Capability with 'dst' referring to a Fiasco.OC IRQ object
		 */
		Native_capability _sem;

	public:

		/**
		 * Constructor
		 */
		Signal_source_client(Cpu_session &, Capability<Signal_source> cap);

		/**
		 * Destructor
		 */
		~Signal_source_client();


		/*****************************
		 ** Signal source interface **
		 *****************************/

		/*
		 * Build with frame pointer to make GDB backtraces work. See issue #1061.
		 */
		__attribute__((optimize("-fno-omit-frame-pointer")))
		Signal wait_for_signal() override;
};

#endif /* _INCLUDE__SIGNAL_SOURCE__CLIENT_H_ */
