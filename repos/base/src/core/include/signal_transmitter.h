/*
 * \brief  Initialization of core-specific signal-delivery mechanism
 * \author Norman Feske
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SIGNAL_TRANSMITTER_H_
#define _CORE__INCLUDE__SIGNAL_TRANSMITTER_H_

namespace Genode {

	class Rpc_entrypoint;

	/*
	 * Initialize the emission of signals originating from the component
	 *
	 * The 'ep' is the entrypoint called by the signal-source clients. On
	 * kernels where signals are delivered by core as IPC-reply messages, we
	 * need to ensure that the replies are sent by the same thread that
	 * received the RPC request (the 'ep' thread). Otherwise, the kernel
	 * (e.g., OKL4, Pistachio, L4/Fiasco) will refuse to send the reply.
	 *
	 * On other kernels, the argument is unused.
	 *
	 * Note that this function is called at a very early stage, before the
	 * global constructors are executed. It should merely remember the 'ep'
	 * argument.
	 */
	void init_core_signal_transmitter(Rpc_entrypoint &ep);
}

#endif /* _CORE__INCLUDE__SIGNAL_TRANSMITTER_H_ */
