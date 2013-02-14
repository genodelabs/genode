/*
 * \brief  Delivery and reception of asynchronous notifications on HW-core
 * \author Martin Stein
 * \date   2012-05-05
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SIGNAL_H__
#define _INCLUDE__BASE__SIGNAL_H__

/* Genode includes */
#include <signal_session/signal_session.h>
#include <base/lock.h>
#include <util/list.h>

namespace Genode
{
	/**
	 * A bunch of asynchronuosly triggered events that target the same context
	 *
	 * Because a signal can trigger asynchronously at a context,
	 * the kernel accumulates them and provides them as such a bunch,
	 * once the receiver indicates that he is ready to receive.
	 */
	class Signal
	{
		unsigned _imprint; /* receiver-local signal-context pointer */
		int      _num; /* how often this signal has been triggered */

		public:

			/**
			 * Construct valid signal
			 */
			Signal(unsigned const imprint, int const num) :
				_imprint(imprint), _num(num) { }

			/***************
			 ** Accessors **
			 ***************/

			Signal_context * context() { return (Signal_context *)_imprint; }

			int num() { return _num; }
	};


	typedef List<Signal_context> Context_list;


	/**
	 * A specific signal type that a transmitter can target when it submits
	 *
	 * One receiver might handle multiple signal contexts,
	 * but a signal context is owned by exactly one signal receiver.
	 */
	class Signal_context : public Context_list::Element
	{
		friend class Signal_receiver;

		Signal_receiver *         _receiver; /* receiver that manages us */
		Lock                      _lock; /* serialize object access */
		Signal_context_capability _cap; /* holds the name of our context
		                                 * kernel-object as 'dst' */

		public:

			/**
			 * Construct context that is not yet managed by a receiver
			 */
			Signal_context() : _receiver(0), _lock(Lock::UNLOCKED) { }

			/**
			 * Destructor
			 */
			virtual ~Signal_context() { }

			/* solely needed to enable one to type a capability with us */
			GENODE_RPC_INTERFACE();
	};


	/**
	 * To submit signals to one specific context
	 *
	 * Multiple transmitters can submit to the same context.
	 */
	class Signal_transmitter
	{
		/* names the targeted context kernel-object with its 'dst' field */
		Signal_context_capability _context;

		public:

			/**
			 * Constructor
			 */
			Signal_transmitter(Signal_context_capability const c =
			                   Signal_context_capability())
			: _context(c) { }

			/**
			 * Trigger a signal 'num' times at the context we target
			 */
			void submit(int const num = 1)
			{ Kernel::submit_signal(_context.dst(), num); }

			/***************
			 ** Accessors **
			 ***************/

			void context(Signal_context_capability const c) { _context = c; }
	};


	/**
	 * Manage multiple signal contexts and receive signals targeted to them
	 */
	class Signal_receiver
	{
			Context_list               _contexts; /* contexts that we manage */
			Lock                       _contexts_lock; /* serialize access to
			                                            * 'contexts' */
			Signal_receiver_capability _cap; /* holds name of our receiver
			                                  * kernel-object as 'dst' */

			/**
			 * Let a context 'c' no longer be managed by us
			 *
			 * Doesn't serialize any member access.
			 */
			void _unsync_dissolve(Signal_context * const c);

		public:

			class Context_already_in_use : public Exception { };
			class Context_not_associated : public Exception { };

			/**
			 * Constructor
			 */
			Signal_receiver();

			/**
			 * Destructor
			 */
			~Signal_receiver();

			/**
			 * Let a context 'c' be managed by us
			 *
			 * \return  Capability wich names the context kernel-object in its
			 *          'dst' field . Can be used as target for transmitters.
			 */
			Signal_context_capability manage(Signal_context * const c);

			/**
			 * If any of our signal contexts is pending
			 */
			bool pending();

			/**
			 * Let a context 'c' no longer be managed by us
			 */
			void dissolve(Signal_context * const c);

			/**
			 * Block on any signal that is triggered at one of our contexts
			 */
			Signal wait_for_signal();
	};


	/**
	 * Abstract interface to be implemented by signal dispatchers
	 */
	struct Signal_dispatcher_base : Signal_context
	{
		virtual void dispatch(unsigned num) = 0;
	};


	/**
	 * Adapter for directing signals to member functions
	 *
	 * This utility associates member functions with signals. It is intended to
	 * be used as a member variable of the class that handles incoming signals
	 * of a certain type. The constructor takes a pointer-to-member to the
	 * signal handling function as argument. If a signal is received at the
	 * common signal reception code, this function will be invoked by calling
	 * 'Signal_dispatcher_base::dispatch'.
	 *
	 * \param T  type of signal-handling class
	 */
	template <typename T>
	class Signal_dispatcher : private Signal_dispatcher_base,
	                          public  Signal_context_capability
	{
		private:

			T &obj;
			void (T::*member) (unsigned);
			Signal_receiver &sig_rec;

		public:

			/**
			 * Constructor
			 *
			 * \param sig_rec     signal receiver to associate the signal
			 *                    handler with
			 * \param obj,member  object and member function to call when
			 *                    the signal occurs
			 */
			Signal_dispatcher(Signal_receiver &sig_rec,
			                  T &obj, void (T::*member)(unsigned))
			:
				Signal_context_capability(sig_rec.manage(this)),
				obj(obj), member(member),
				sig_rec(sig_rec)
			{ }

			~Signal_dispatcher() { sig_rec.dissolve(this); }

			void dispatch(unsigned num) { (obj.*member)(num); }
	};
}

#endif /* _INCLUDE__BASE__SIGNAL_H__ */

