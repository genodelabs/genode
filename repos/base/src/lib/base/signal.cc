/*
 * \brief  Generic implementation parts of the signaling framework
 * \author Norman Feske
 * \date   2008-09-16
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/env.h>
#include <base/signal.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <base/trace/events.h>
#include <signal_source/client.h>
#include <util/volatile_object.h>

/* base-internal includes */
#include <base/internal/globals.h>

using namespace Genode;

class Signal_handler_thread : Thread, Lock
{
	private:

		/**
		 * Actual signal source
		 *
		 * Member must be constructed in the context of the signal handler
		 * thread because on some platforms (e.g., Fiasco.OC), the calling
		 * thread context is used for implementing the signal-source protocol.
		 */
		Lazy_volatile_object<Signal_source_client> _signal_source;

		void entry()
		{
			_signal_source.construct(env()->pd_session()->alloc_signal_source());
			unlock();
			Signal_receiver::dispatch_signals(&(*_signal_source));
		}

		enum { STACK_SIZE = 4*1024*sizeof(addr_t) };

	public:

		/**
		 * Constructor
		 */
		Signal_handler_thread(Env &env)
		: Thread(env, "signal handler", STACK_SIZE), Lock(Lock::LOCKED)
		{
			start();

			/*
			 * Make sure the signal source was constructed before proceeding
			 * with the use of signals. Otherwise, signals may get lost until
			 * the construction finished.
			 */
			lock();
		}

		~Signal_handler_thread()
		{
			env()->pd_session()->free_signal_source(*_signal_source);
		}
};


/*
 * The signal-handler thread will be constructed before global constructors are
 * called and, consequently, must not be a global static object. Otherwise, the
 * Lazy_volatile_object constructor will be executed twice.
 */
static Lazy_volatile_object<Signal_handler_thread> & signal_handler_thread()
{
	static Lazy_volatile_object<Signal_handler_thread> inst;
	return inst;
}


namespace Genode {

	/*
	 * Initialize the component-local signal-handling thread
	 *
	 * This function is called once at the startup of the component. It must
	 * be called before creating the first signal receiver.
	 *
	 * We allow this function to be overridden in to enable core to omit the
	 * creation of the signal thread.
	 */
	void init_signal_thread(Env &env) __attribute__((weak));
	void init_signal_thread(Env &env)
	{
		signal_handler_thread().construct(env);
	}

	void destroy_signal_thread()
	{
		signal_handler_thread().destruct();
	}
}


/*****************************
 ** Signal context registry **
 *****************************/

namespace Genode {

	/**
	 * Facility to validate the liveliness of signal contexts
	 *
	 * After dissolving a 'Signal_context' from a 'Signal_receiver', a signal
	 * belonging to the context may still in flight, i.e., currently processed
	 * within core or the kernel. Hence, after having received a signal, we
	 * need to manually check for the liveliness of the associated context.
	 * Because we cannot trust the signal imprint to represent a valid pointer,
	 * we need an associative data structure to validate the value. That is the
	 * role of the 'Signal_context_registry'.
	 */
	class Signal_context_registry
	{
		private:

			/*
			 * Currently, the registry is just a linked list. If this becomes a
			 * scalability problem, we might introduce a more sophisticated
			 * associative data structure.
			 */
			Lock mutable                        _lock;
			List<List_element<Signal_context> > _list;

		public:

			void insert(List_element<Signal_context> *le)
			{
				Lock::Guard guard(_lock);
				_list.insert(le);
			}

			void remove(List_element<Signal_context> *le)
			{
				Lock::Guard guard(_lock);
				_list.remove(le);
			}

			bool test_and_lock(Signal_context *context) const
			{
				Lock::Guard guard(_lock);

				/* search list for context */
				List_element<Signal_context> const *le = _list.first();
				for ( ; le; le = le->next()) {

					if (context == le->object()) {
						/* lock object */
						context->_lock.lock();
						return true;
					}
				}
				return false;
			}
	};
}


/**
 * Return process-wide registry of registered signal contexts
 */
Genode::Signal_context_registry *signal_context_registry()
{
	static Signal_context_registry inst;
	return &inst;
}


/********************
 ** Signal context **
 ********************/

void Signal_context::submit(unsigned num)
{
	if (!_receiver) {
		PWRN("signal context with no receiver");
		return;
	}

	if (!signal_context_registry()->test_and_lock(this)) {
		PWRN("encountered dead signal context");
		return;
	}

	/* construct and locally submit signal object */
	Signal::Data signal(this, num);
	_receiver->local_submit(signal);

	/* free context lock that was taken by 'test_and_lock' */
	_lock.unlock();
}


/*********************
 ** Signal receiver **
 *********************/

Signal_receiver::Signal_receiver() { }


Signal_context_capability Signal_receiver::manage(Signal_context *context)
{
	if (context->_receiver)
		throw Context_already_in_use();

	context->_receiver = this;

	Lock::Guard list_lock_guard(_contexts_lock);

	/* insert context into context list */
	_contexts.insert(&context->_receiver_le);

	/* register context at process-wide registry */
	signal_context_registry()->insert(&context->_registry_le);

	retry<Pd_session::Out_of_metadata>(
		[&] () {
			/* use signal context as imprint */
			context->_cap = env()->pd_session()->alloc_context(_cap, (long)context);
		},
		[&] () {
			size_t const quota = 1024*sizeof(long);
			char buf[64];
			snprintf(buf, sizeof(buf), "ram_quota=%zu", quota);

			PINF("upgrading quota donation for PD session (%zu bytes)", quota);

			env()->parent()->upgrade(env()->pd_session_cap(), buf);
		}
	);

	return context->_cap;
}


void Signal_receiver::block_for_signal()
{
	_signal_available.down();
}


void Signal_receiver::local_submit(Signal::Data ns)
{
	Signal_context *context = ns.context;

	/*
	 * Replace current signal of the context by signal with accumulated
	 * counters. In the common case, the current signal is an invalid
	 * signal with a counter value of zero.
	 */
	unsigned num = context->_curr_signal.num + ns.num;
	context->_curr_signal = Signal::Data(context, num);

	/* wake up the receiver if the context becomes pending */
	if (!context->_pending) {
		context->_pending = true;
		_signal_available.up();
	}
}


void Signal_receiver::dispatch_signals(Signal_source *signal_source)
{
	for (;;) {
		Signal_source::Signal source_signal = signal_source->wait_for_signal();

		/* look up context as pointed to by the signal imprint */
		Signal_context *context = (Signal_context *)(source_signal.imprint());

		if (!context) {
			PERR("received null signal imprint, stop signal handling");
			sleep_forever();
		}

		if (!signal_context_registry()->test_and_lock(context)) {
			PWRN("encountered dead signal context");
			continue;
		}

		if (context->_receiver) {
			/* construct and locally submit signal object */
			Signal::Data signal(context, source_signal.num());
			context->_receiver->local_submit(signal);
		} else {
			PWRN("signal context with no receiver");
		}

		/* free context lock that was taken by 'test_and_lock' */
		context->_lock.unlock();
	}
}


void Signal_receiver::_platform_begin_dissolve(Signal_context *) { }


void Signal_receiver::_platform_finish_dissolve(Signal_context * const c) {
	signal_context_registry()->remove(&c->_registry_le); }


void Signal_receiver::_platform_destructor() { }
