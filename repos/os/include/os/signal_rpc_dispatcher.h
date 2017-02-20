/*
 * \brief  Utility for dispatching signals at at RPC entrypoint
 * \author Norman Feske
 * \date   2013-09-07
 *
 * \deprecated  This header merely exists to maintain API compatibility
 *              to Genode 15.11. Its functionality moved to base/signal.h.
 *              The header will eventually be removed.
 */

#warning os/signal_rpc_dispatcher.h is deprecated

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_
#define _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_

#include <base/signal.h>

namespace Genode { template <typename, typename> class Signal_rpc_member; }


/**
 * Signal dispatcher for handling signals by an object method
 *
 * This utility associates object methods with signals. It is intended to
 * be used as a member variable of the class that handles incoming signals
 * of a certain type. The constructor takes a pointer-to-member to the
 * signal-handling method as argument. If a signal is received at the
 * common signal reception code, this method will be invoked by calling
 * 'Signal_dispatcher_base::dispatch'.
 *
 * \param T  type of signal-handling class
 * \param EP type of entrypoint handling signal RPC
 *
 * \deprecated  this class template is superseded by the 'Signal_handler'
 *              in base/signal.h
 */
template <typename T, typename EP = Genode::Entrypoint>
struct Genode::Signal_rpc_member : Genode::Signal_dispatcher_base,
                                   Genode::Signal_context_capability
{
	EP &ep;
	T  &obj;
	void (T::*member) (unsigned);

	/**
	 * Constructor
	 *
	 * \param ep          entrypoint managing this signal RPC
	 * \param obj,member  object and method to call when
	 *                    the signal occurs
	 */
	Signal_rpc_member(EP &ep, T &obj, void (T::*member)(unsigned))
	: Signal_context_capability(ep.manage(*this)),
	  ep(ep), obj(obj), member(member) { }

	~Signal_rpc_member() { ep.dissolve(*this); }

	/**
	 * Interface of Signal_dispatcher_base
	 */
	void dispatch(unsigned num) { (obj.*member)(num); }
};

#endif /* _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_ */
