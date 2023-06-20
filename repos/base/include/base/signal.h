/*
 * \brief  Delivery and reception of asynchronous notifications
 * \author Norman Feske
 * \date   2008-09-05
 *
 * Each transmitter sends signals to one fixed destination.
 * A receiver can receive signals from multiple sources.
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SIGNAL_H_
#define _INCLUDE__BASE__SIGNAL_H_

#include <util/noncopyable.h>
#include <util/list.h>
#include <base/semaphore.h>
#include <base/capability.h>
#include <base/exception.h>

/* only needed for base-hw */
namespace Kernel { struct Signal_receiver; }

namespace Genode {

	class Entrypoint;
	class Rpc_entrypoint;
	class Pd_session;
	class Signal_source;

	class Signal_receiver;
	class Signal_context;
	class Signal_context_registry;
	class Signal_transmitter;
	class Signal;
	class Signal_dispatcher_base;

	template <typename, typename> class Signal_handler;
	template <typename, typename> class Io_signal_handler;

	typedef Capability<Signal_context> Signal_context_capability;
}


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
class Genode::Signal
{
	private:

		struct Data
		{
			Signal_context *context;
			unsigned        num;

			/**
			 * Constructor
			 *
			 * \param context  signal context specific for the
			 *                 signal-receiver capability used for signal
			 *                 transmission
			 * \param num      number of signals received from the same
			 *                 transmitter
			 */
			Data(Signal_context *context, unsigned num)
			: context(context), num(num) { }

			/**
			 * Default constructor, representing an invalid signal
			 */
			Data() : context(0), num(0) { }

		} _data { };

		/**
		 * Constructor for invalid signal
		 */
		Signal() { };

		/**
		 * Constructor
		 *
		 * Signal objects are constructed by signal receivers only.
		 */
		Signal(Data data);

		friend class Kernel::Signal_receiver;
		friend class Signal_receiver;
		friend class Signal_context;

		void _dec_ref_and_unlock();
		void _inc_ref();

	public:

		Signal(Signal const &other);

		/**
		 * \noapi
		 */
		Signal &operator = (Signal const &other);

		~Signal();

		Signal_context *context()       { return _data.context; }
		unsigned        num()     const { return _data.num; }
		bool            valid()   const { return _data.context != nullptr; }
};


/**
 * Signal transmitter
 *
 * Each signal-transmitter instance acts on behalf the context specified
 * as constructor argument. Therefore, the resources needed for the
 * transmitter such as the consumed memory 'sizeof(Signal_transmitter)'
 * should be accounted to the owner of the context.
 */
class Genode::Signal_transmitter
{
	private:

		Signal_context_capability _context { };  /* destination */

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
		 * Return signal context
		 */
		Signal_context_capability context();

		/**
		 * Trigger signal submission to context
		 *
		 * \param cnt  number of signals to submit at once
		 */
		void submit(unsigned cnt = 1);
};


/**
 * Signal context
 *
 * A signal context is a destination for signals. One receiver can listen
 * to multple contexts. If a signal arrives, the context is provided with the
 * signal. This enables the receiver to distinguish different signal sources
 * and dispatch incoming signals context-specific.
 *
 * Signal contexts are classified to represent one of two levels: application
 * and I/O. The signal level determines how a signal is handled by
 * 'wait_and_dispatch_one_io_signal', which defers signals corresponding to
 * application-level contexts and dispatches only I/O-level signals.
 */
class Genode::Signal_context : Interface, Noncopyable
{
	public:

		enum class Level { App, Io };

	private:

		/**
		 * List element in 'Signal_receiver'
		 */
		Signal_context mutable *_next { nullptr };
		Signal_context mutable *_prev { nullptr };

		/**
		 * List element in process-global registry
		 */
		List_element<Signal_context> _registry_le { this };

		/**
		 * List element in deferred application signal list
		 */
		List_element<Signal_context> _deferred_le { this };

		/**
		 * Receiver to which the context is associated with
		 *
		 * This member is initialized by the receiver when associating
		 * the context with the receiver via the 'cap' method.
		 */
		Signal_receiver *_receiver { nullptr };

		Mutex        _mutex         { };   /* protect '_curr_signal' */
		Signal::Data _curr_signal   { };   /* most-currently received signal */
		bool         _pending { false };   /* current signal is valid */
		unsigned int _ref_cnt     { 0 };   /* number of references to context */
		Mutex        _destroy_mutex { };   /* prevent destruction while the
		                                      context is in use */

		/**
		 * Capability assigned to this context after being assocated with
		 * a 'Signal_receiver' via the 'manage' method. We store this
		 * capability in the 'Signal_context' for the mere reason to
		 * properly destruct the context (see '_unsynchronized_dissolve').
		 */
		Signal_context_capability _cap { };

		friend class Signal;
		friend class Signal_receiver;
		friend class Signal_context_registry;

		/*
		 * Noncopyable
		 */
		Signal_context &operator = (Signal_context const &);
		Signal_context(Signal_context const &);

	protected:

		Level _level = Level::App;

	public:

		Signal_context() { }

		/**
		 * Destructor
		 *
		 * The virtual destructor is just there to generate a vtable for
		 * signal-context objects such that signal contexts can be dynamically
		 * casted.
		 */
		virtual ~Signal_context();

		Level level() const { return _level; }

		List_element<Signal_context> *deferred_le() { return &_deferred_le; }

		void local_submit();

		/*
		 * Signal contexts are never invoked but only used as arguments for
		 * 'Signal_session' methods. Hence, there exists a capability
		 * type for it but no real RPC interface.
		 */
		GENODE_RPC_INTERFACE();
};


/**
 * Signal receiver
 */
class Genode::Signal_receiver : Noncopyable
{
	private:

		/**
		 * A list where the head can be moved
		 */
		class Context_ring
		{
			private:

				Signal_context *_head { nullptr };

			public:

				struct Break_for_each : Exception { };

				Signal_context *head() const { return _head; }

				void head(Signal_context *re) { _head = re; }

				void insert_as_tail(Signal_context *re);

				void remove(Signal_context const *re);

				template <typename FUNC>
				void for_each_locked(FUNC && functor) const
				{
					Signal_context *context = _head;
					if (!context) return;

					do {
						Mutex::Guard mutex_guard(context->_mutex);
						if (functor(*context)) return;
						context = context->_next;
					} while (context != _head);
				}
		};

		Pd_session &_pd;

		/**
		 * Semaphore used to indicate that signal(s) are ready to be picked
		 * up. This is needed for platforms other than 'base-hw' only.
		 */
		Semaphore _signal_available { };

		/**
		 * Provides the kernel-object name via the 'dst' method. This is
		 * needed for 'base-hw' only.
		 */
		Capability<Signal_source> _cap { };

		/**
		 * List of associated contexts
		 */
		Mutex        _contexts_mutex { };
		Context_ring _contexts       { };

		/**
		 * Helper to dissolve given context
		 *
		 * This method prevents duplicated code in '~Signal_receiver'
		 * and 'dissolve'. Note that '_contexts_mutex' must be held when
		 * calling this method.
		 */
		void _unsynchronized_dissolve(Signal_context *context);

		/**
		 * Hook to platform specific destructor parts
		 */
		void _platform_destructor();

		/**
		 * Hooks to platform specific dissolve parts
		 */
		void _platform_begin_dissolve(Signal_context * const c);
		void _platform_finish_dissolve(Signal_context * const c);

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
		 * Block until a signal is received and return the signal
		 *
		 * \return received signal
		 */
		Signal wait_for_signal();

		/**
		 * Block until a signal is received
		 */
		void block_for_signal();

		/**
		 * Unblock signal waiter
		 */
		void unblock_signal_waiter(Rpc_entrypoint &rpc_ep);

		/**
		 * Retrieve pending signal
		 *
		 * \return  received signal (invalid if no pending signal found)
		 */
		Signal pending_signal();

		/**
		 * Locally submit signal to the receiver
		 *
		 * \noapi
		 */
		void local_submit(Signal::Data signal);

		/**
		 * Framework-internal signal-dispatcher
		 *
		 * \noapi
		 *
		 * This method is called from the thread that monitors the signal
		 * source associated with the process. It must not be used for other
		 * purposes.
		 */
		static void dispatch_signals(Signal_source *);
};


/**
 * Abstract interface to be implemented by signal dispatchers
 */
struct Genode::Signal_dispatcher_base : Signal_context
{
	virtual void dispatch(unsigned num) = 0;
};


/**
 * Signal dispatcher for handling signals by an object method
 *
 * This utility associates an object method with signals. It is intended to
 * be used as a member variable of the class that handles incoming signals
 * of a certain type. The constructor takes a pointer-to-member to the
 * signal-handling method as argument.
 *
 * \param T  type of signal-handling class
 * \param EP type of entrypoint handling signal RPC
 */
template <typename T, typename EP = Genode::Entrypoint>
class Genode::Signal_handler : public Signal_dispatcher_base
{
	private:

		Signal_context_capability _cap;
		EP &_ep;
		T  &_obj;
		void (T::*_member) ();

		/*
		 * Noncopyable
		 */
		Signal_handler(Signal_handler const &);
		Signal_handler &operator = (Signal_handler const &);

	public:

		/**
		 * Constructor
		 *
		 * \param ep          entrypoint managing this signal RPC
		 * \param obj,member  object and method to call when
		 *                    the signal occurs
		 */
		Signal_handler(EP &ep, T &obj, void (T::*member)())
		:
			_cap(ep.manage(*this)), _ep(ep), _obj(obj), _member(member)
		{ }

		~Signal_handler() { _ep.dissolve(*this); }

		/**
		 * Interface of Signal_dispatcher_base
		 */
		void dispatch(unsigned) override { (_obj.*_member)(); }

		operator Capability<Signal_context>() const { return _cap; }
};


/**
 * Signal handler for I/O-level signals
 */
template <typename T, typename EP = Genode::Entrypoint>
struct Genode::Io_signal_handler : Signal_handler<T, EP>
{
	Io_signal_handler(EP &ep, T &obj, void (T::*member)())
	: Signal_handler<T, EP>(ep, obj, member)
	{ Signal_context::_level = Signal_context::Level::Io; }
};

#endif /* _INCLUDE__BASE__SIGNAL_H_ */
