/*
 * \brief  Genode/Muen specific VirtualBox SUPLib supplements
 * \author Adrian-Ken Rueegsegger
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _VIRTUALBOX__SPEC__MUEN__VM_HANDLER_H_
#define _VIRTUALBOX__SPEC__MUEN__VM_HANDLER_H_

#include <base/signal.h>

#include <vm_session/vm_session.h>
#include <vm_session/connection.h>

namespace Genode
{
	/**
	 * Vm execution handler.
	 */
	class Vm_handler;
}


class Genode::Vm_handler
{
	private:

		Vm_connection _vm_session;
		Signal_context_capability _sig_cap;
		Signal_receiver _sig_rcv;
		Signal_transmitter _sig_xmit;
		Signal_context _sig_ctx;

	public:

		Vm_handler(Genode::Env &env)
		:
			_vm_session(env)
		{
			_sig_cap = _sig_rcv.manage(&_sig_ctx);
			_sig_xmit.context(_sig_cap);
			_vm_session.exception_handler(_sig_cap);
		}

		~Vm_handler() { _sig_rcv.dissolve(&_sig_ctx); }

		/**
		 * Starts execution of the Vm and blocks until the Vm returns or the
		 * execution handler gets poked.
		 */
		void run_vm()
		{
			_vm_session.run();
			_sig_rcv.wait_for_signal();
		}
};

#endif /* _VIRTUALBOX__SPEC__MUEN__VM_HANDLER_H_ */
