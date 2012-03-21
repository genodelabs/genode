/*
 * \brief  Delivery and reception of asynchronous notifications
 * \author Norman Feske
 * \date   2008-09-05
 *
 * Each transmitter sends signals to one fixed destination.
 * A receiver can receive signals from multiple sources.
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SIGNAL_H__
#define _INCLUDE__BASE__SIGNAL_H__

#include <base/semaphore.h>
#include <signal_session/signal_session.h>

namespace Genode {

	class Signal_receiver;
	class Signal_context;
	class Signal_context_registry;


	/**
	 * Signal
	 *
	 * A signal represents a number of asynchronous notifications produced by
	 * one transmitter. If notifications are generated at a higher rate than as
	 * they can be processed at the receiver, the transmitter counts the
	 * notifications and delivers the total amount with the next signal
	 * transmission. This way, the total number of notifications gets properly
	 * communicated to the receiver even if the receiver is not highly
	 * responsive.
	 *
	 * Asynchronous notifications do not carry any payload because this payload
	 * would need to be queued at the transmitter. However, each transmitter
	 * imprints a signal-context reference into each signal. This context
	 * can be used by the receiver to distinguish signals coming from different
	 * transmitters.
	 */
	class Signal
	{
		private:

			friend class Signal_receiver;
			friend class Signal_context;

			Signal_context *_context;
			int             _num;

			/**
			 * Constructor
			 *
			 * \param context  signal context specific for the signal-receiver
			 *                 capability used for signal transmission
			 * \param num      number of signals received from the same transmitter
			 *
			 * Signal objects are constructed only by signal receivers.
			 */
			Signal(Signal_context *context, int num)
			: _context(context), _num(num)
			{ }

		public:

			/**
			 * Default constructor, creating an invalid signal
			 */
			Signal() : _context(0), _num(0) { }

			/**
			 * Return signal context
			 */
			Signal_context *context() { return _context; }

			/**
			 * Return number of signals received from the same transmitter
			 */
			int num() { return _num; }
	};

	/**
	 * Signal context
	 *
	 * A signal context is a destination for signals. One receiver can listen
	 * to multple contexts. If a signal arrives, the context is provided with the
	 * signel. This enables the receiver to distinguish different signal sources
	 * and dispatch incoming signals context-specific.
	 */
	class Signal_context
	{
		private:

			/**
			 * List element in 'Signal_receiver'
			 */
			List_element<Signal_context> _receiver_le;

			/**
			 * List element in process-global registry
			 */
			List_element<Signal_context> _registry_le;

			/**
			 * Receiver to which the context is associated with
			 *
			 * This member is initialized by the receiver when associating
			 * the context with the receiver via the 'cap' function.
			 */
			Signal_receiver *_receiver;

			Lock   _lock;          /* protect '_curr_signal'         */
			Signal _curr_signal;   /* most-currently received signal */
			bool   _pending;       /* current signal is valid        */

			/**
			 * Capability assigned to this context after being assocated with
			 * a 'Signal_receiver' via the 'manage' function. We store this
			 * capability in the 'Signal_context' for the mere reason to
			 * properly destruct the context (see '_unsynchronized_dissolve').
			 */
			Signal_context_capability _cap;

			friend class Signal_receiver;
			friend class Signal_context_registry;

		public:

			/**
			 * Constructor
			 */
			Signal_context()
			: _receiver_le(this), _registry_le(this),
			  _receiver(0), _pending(0) { }

			/**
			 * Destructor
			 *
			 * The virtual destructor is just there to generate a vtable for
			 * signal-context objects such that signal contexts can be dynamically
			 * casted.
			 */
			virtual ~Signal_context() { }

			/*
			 * Signal contexts are never invoked but only used as arguments for
			 * 'Signal_session' functions. Hence, there exists a capability
			 * type for it but no real RPC interface.
			 */
			GENODE_RPC_INTERFACE();
	};


	/**
	 * Signal transmitter
	 *
	 * Each signal-transmitter instance acts on behalf the context specified
	 * as constructor argument. Therefore, the resources needed for the
	 * transmitter such as the consumed memory 'sizeof(Signal_transmitter)'
	 * should be accounted to the owner of the context.
	 */
	class Signal_transmitter
	{
		private:

			Signal_context_capability _context;  /* destination */

		public:

			/**
			 * Constructor
			 *
			 * \param context  capability to signal context that is going to
			 *                 receive signals produced by the transmitter
			 */
			Signal_transmitter(Signal_context_capability context = Signal_context_capability());

			/**
			 * Set signal context
			 */
			void context(Signal_context_capability context);

			/**
			 * Trigger signal submission to context
			 *
			 * \param cnt  number of signals to submit at once
			 */
			void submit(int cnt = 1);
	};


	/**
	 * Signal receiver
	 */
	class Signal_receiver
	{
		private:

			Semaphore _signal_available;  /* signal(s) awaiting to be picked up */

			/**
			 * List of associated contexts
			 */
			Lock                                _contexts_lock;
			List<List_element<Signal_context> > _contexts;

			/**
			 * Helper to dissolve given context
			 *
			 * This function prevents duplicated code in '~Signal_receiver'
			 * and 'dissolve'. Note that '_contexts_lock' must be held when
			 * calling this function.
			 */
			void _unsynchronized_dissolve(Signal_context *context);

		public:

			/**
			 * Exception class
			 */
			class Context_already_in_use { };
			class Context_not_associated { };

			/**
			 * Constructor
			 */
			Signal_receiver();

			/**
			 * Destructor
			 */
			~Signal_receiver();

			/**
			 * Manage signal context and return new signal-context capability
			 *
			 * \param context  context associated with signals delivered to the
			 *                 receiver
			 * \throw          'Context_already_in_use'
			 * \return         new signal-context capability that can be
			 *                 passed to a signal transmitter
			 */
			Signal_context_capability manage(Signal_context *context);

			/**
			 * Dissolve signal context from receiver
			 *
			 * \param context  context to remove from receiver
			 * \throw          'Context_not_associated'
			 */
			void dissolve(Signal_context *context);

			/**
			 * Return true if signal was received
			 */
			bool pending();

			/**
			 * Block until a signal is received
			 *
			 * \return received signal
			 */
			Signal wait_for_signal();

			/**
			 * Locally submit signal to the receiver
			 */
			void local_submit(Signal signal);

			/**
			 * Framework-internal signal-dispatcher
			 *
			 * This function is called from the thread that monitors the signal
			 * source associated with the process. It must not be used for other
			 * purposes.
			 */
			static void dispatch_signals(Signal_source *signal_source);
	};
}

#endif /* _INCLUDE__BASE__SIGNAL_H__ */
