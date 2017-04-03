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

/* only needed for base-hw */
namespace Kernel { struct Signal_receiver; }

namespace Genode {

	class Entrypoint;
	class Rpc_entrypoint;
	class Signal_source;

	class Signal_receiver;
	class Signal_context;
	class Signal_context_registry;
	class Signal_transmitter;
	class Signal;
	class Signal_dispatcher_base;

	template <typename>           class Signal_dispatcher;
	template <typename>           class Io_signal_dispatcher;
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

		} _data;

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
 * Signal receiver
 */
class Genode::Signal_receiver : Noncopyable
{
	private:

		/**
		 * Semaphore used to indicate that signal(s) are ready to be picked
		 * up. This is needed for platforms other than 'base-hw' only.
		 */
		Semaphore _signal_available;

		/**
		 * Provides the kernel-object name via the 'dst' method. This is
		 * needed for 'base-hw' only.
		 */
		Capability<Signal_source> _cap;

		/**
		 * List of associated contexts
		 */
		Lock                                _contexts_lock;
		List<List_element<Signal_context> > _contexts;

		/**
		 * Helper to dissolve given context
		 *
		 * This method prevents duplicated code in '~Signal_receiver'
		 * and 'dissolve'. Note that '_contexts_lock' must be held when
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
		class Signal_not_pending     { };

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
		 * Retrieve  pending signal
		 *
		 * \throw   'Signal_not_pending' no pending signal found
		 * \return  received signal
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
class Genode::Signal_context
{
	public:

		enum class Level { App, Io };

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
		 * List element in deferred application signal list
		 */
		List_element<Signal_context> _deferred_le;

		/**
		 * Receiver to which the context is associated with
		 *
		 * This member is initialized by the receiver when associating
		 * the context with the receiver via the 'cap' method.
		 */
		Signal_receiver *_receiver;

		Lock         _lock;          /* protect '_curr_signal'         */
		Signal::Data _curr_signal;   /* most-currently received signal */
		bool         _pending;       /* current signal is valid        */
		unsigned int _ref_cnt;       /* number of references to this context */
		Lock         _destroy_lock;  /* prevent destruction while the
		                                context is in use */

		/**
		 * Capability assigned to this context after being assocated with
		 * a 'Signal_receiver' via the 'manage' method. We store this
		 * capability in the 'Signal_context' for the mere reason to
		 * properly destruct the context (see '_unsynchronized_dissolve').
		 */
		Signal_context_capability _cap;

		friend class Signal;
		friend class Signal_receiver;
		friend class Signal_context_registry;

	protected:

		Level _level = Level::App;

	public:

		/**
		 * Constructor
		 */
		Signal_context()
		: _receiver_le(this), _registry_le(this), _deferred_le(this),
		  _receiver(0), _pending(0), _ref_cnt(0) { }

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

		/**
		 * Local signal submission (DEPRECATED)
		 *
		 * \noapi
		 *
		 * Trigger local signal submission (within the same address space), the
		 * context has to be bound to a sginal receiver beforehand.
		 *
		 * \param num  number of pending signals
		 */
		void submit(unsigned num);

		/*
		 * Signal contexts are never invoked but only used as arguments for
		 * 'Signal_session' methods. Hence, there exists a capability
		 * type for it but no real RPC interface.
		 */
		GENODE_RPC_INTERFACE();
};


/**
 * Abstract interface to be implemented by signal dispatchers
 */
struct Genode::Signal_dispatcher_base : Signal_context
{
	virtual void dispatch(unsigned num) = 0;
};


/**
 * Adapter for directing signals to object methods
 *
 * This utility associates object methods with signals. It is intended to
 * be used as a member variable of the class that handles incoming signals
 * of a certain type. The constructor takes a pointer-to-member to the
 * signal handling method as argument. If a signal is received at the
 * common signal reception code, this method will be invoked by calling
 * 'Signal_dispatcher_base::dispatch'.
 *
 * \param T  type of signal-handling class
 */
template <typename T>
class Genode::Signal_dispatcher : public Signal_dispatcher_base,
                                  public Signal_context_capability
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
		 * \param obj,member  object and method to call when
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

		void dispatch(unsigned num) override { (obj.*member)(num); }
};


/**
 * Signal dispatcher for I/O-level signals
 */
template <typename T>
struct Genode::Io_signal_dispatcher : Signal_dispatcher<T>
{
	Io_signal_dispatcher(Signal_receiver &sig_rec,
	                     T &obj, void (T::*member)(unsigned))
	: Signal_dispatcher<T>(sig_rec, obj, member)
	{ Signal_context::_level = Signal_context::Level::Io; }
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
struct Genode::Signal_handler : Genode::Signal_dispatcher_base,
                                Genode::Signal_context_capability
{
	EP &ep;
	T  &obj;
	void (T::*member) ();

	/**
	 * Constructor
	 *
	 * \param ep          entrypoint managing this signal RPC
	 * \param obj,member  object and method to call when
	 *                    the signal occurs
	 */
	Signal_handler(EP &ep, T &obj, void (T::*member)())
	: Signal_context_capability(ep.manage(*this)),
	  ep(ep), obj(obj), member(member) { }

	~Signal_handler() { ep.dissolve(*this); }

	/**
	 * Interface of Signal_dispatcher_base
	 */
	void dispatch(unsigned num) override { (obj.*member)(); }
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
