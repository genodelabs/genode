/*
 * \brief  Utility for dispatching signals via an RPC entrypoint **
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_
#define _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_

#include <base/rpc_server.h>
#include <base/signal.h>

namespace Genode {

	class Signal_rpc_dispatcher_base;
	template <typename> class Signal_rpc_functor;
	template <typename, typename> class Signal_rpc_member;

	template <typename FUNCTOR>
	Signal_rpc_functor<FUNCTOR> signal_rpc_functor(FUNCTOR &);
}


struct Genode::Signal_rpc_dispatcher_base : Genode::Signal_dispatcher_base
{
	private:

		struct Proxy
		{
			GENODE_RPC(Rpc_handle_signal, void, handle_signal, unsigned);
			GENODE_RPC_INTERFACE(Rpc_handle_signal);
		};

		struct Proxy_component : Genode::Rpc_object<Proxy, Proxy_component>
		{
			Genode::Signal_rpc_dispatcher_base &_dispatcher;

			Proxy_component(Signal_rpc_dispatcher_base &dispatcher)
			: _dispatcher(dispatcher) { }

			void handle_signal(unsigned num)
			{
				_dispatcher.dispatch_at_entrypoint(num);
			}
		};

		Proxy_component   _proxy;
		Capability<Proxy> _proxy_cap;
		unsigned          _nesting_level;

	private:

		/**
		 * To be implemented by the derived class
		 */
		virtual void dispatch_at_entrypoint(unsigned num) = 0;

	protected:

		Signal_rpc_dispatcher_base() : _proxy(*this), _nesting_level(0) { }

		Capability<Proxy> proxy_cap() { return _proxy_cap; }

	public:

		/**
		 * Associate signal dispatcher with entrypoint
		 */
		Signal_context_capability manage(Signal_receiver &sig_rec, Rpc_entrypoint &ep)
		{
			_proxy_cap = ep.manage(&_proxy);
			return sig_rec.manage(this);
		}

		/**
		 * Disassociate signal dispatcher from entrypoint
		 */
		void dissolve(Signal_receiver &sig_rec, Rpc_entrypoint &ep)
		{
			ep.dissolve(&_proxy);
			_proxy_cap = Capability<Proxy>();
			sig_rec.dissolve(this);
		}

	public:

		/**
		 * Interface of Signal_dispatcher_base
		 */
		void dispatch(unsigned num)
		{
			/*
			 * Keep track of nesting levels to deal with nested signal
			 * dispatching. When called from within the RPC entrypoint, any
			 * attempt to perform a RPC call would lead to a deadlock. In
			 * this case, we call the 'dispatch' function directly.
			 */
			_nesting_level++;

			/* called from the signal-receiving thread */
			if (_nesting_level == 1)
				proxy_cap().call<Proxy::Rpc_handle_signal>(num);

			/* called from the context of the RPC entrypoint */
			if (_nesting_level > 1)
				dispatch_at_entrypoint(num);

			_nesting_level--;
		}
};


/**
 * Signal dispatcher that executes the signal handling code in the context
 * of an RPC entrypoint
 *
 * The 'Signal_rpc_dispatcher' provides an easy way for a server to serialize
 * the handling of signals with incoming RPC requests. Incoming signals are
 * delegated to the RPC entrypoint via a local RPC call. The signal handling
 * code is then executed in the context of the RPC entrypoint.
 */
template <typename FUNCTOR>
struct Genode::Signal_rpc_functor : Genode::Signal_rpc_dispatcher_base
{
	FUNCTOR &functor;

	/**
	 * Constructor
	 *
	 * \param func  functor taking containing the signal-handling code
	 *
	 * The functor 'func' has the signature 'void func(unsigned num)'
	 * whereas 'num' is the number of signals received at once.
	 */
	Signal_rpc_functor(FUNCTOR &functor) : functor(functor) { }

	/**
	 * Interface of Signal_rpc_dispatcher_base
	 */
	void dispatch_at_entrypoint(unsigned num) { functor(num); }
};


namespace Server{
	class Entrypoint;
}


/**
 * Signal dispatcher for directing signals via RPC to member function
 *
 * This utility associates member functions with signals. It is intended to
 * be used as a member variable of the class that handles incoming signals
 * of a certain type. The constructor takes a pointer-to-member to the
 * signal handling function as argument. If a signal is received at the
 * common signal reception code, this function will be invoked by calling
 * 'Signal_dispatcher_base::dispatch'.
 *
 * \param T  type of signal-handling class
 * \param EP type of entrypoint handling signal RPC
 */
template <typename T, typename EP = Server::Entrypoint>
struct Genode::Signal_rpc_member : Genode::Signal_rpc_dispatcher_base,
                                   Genode::Signal_context_capability
{
	EP &ep;
	T  &obj;
	void (T::*member) (unsigned);

	/**
	 * Constructor
	 *
	 * \param ep          entrypoint managing this signal RPC
	 * \param obj,member  object and member function to call when
	 *                    the signal occurs
	 */
	Signal_rpc_member(EP &ep, T &obj, void (T::*member)(unsigned))
	: Signal_context_capability(ep.manage(*this)),
	  ep(ep), obj(obj), member(member) { }

	~Signal_rpc_member() { ep.dissolve(*this); }

	/**
	 * Interface of Signal_rpc_dispatcher_base
	 */
	void dispatch_at_entrypoint(unsigned num) { (obj.*member)(num); }
};


/**
 * Convenience utility for creating 'Signal_rpc_dispatcher' objects
 */
template <typename FUNCTOR>
Genode::Signal_rpc_functor<FUNCTOR> Genode::signal_rpc_functor(FUNCTOR &func)
{
	return Signal_rpc_functor<FUNCTOR>(func);
}

#endif /* _INCLUDE__OS__SIGNAL_RPC_DISPATCHER_H_ */
