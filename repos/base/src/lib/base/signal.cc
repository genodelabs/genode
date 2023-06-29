/*
 * \brief  Generic implementation parts of the signaling framework
 * \author Norman Feske
 * \date   2008-09-16
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/env.h>
#include <base/signal.h>
#include <base/thread.h>
#include <base/sleep.h>
#include <base/trace/events.h>
#include <util/reconstructible.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <base/internal/unmanaged_singleton.h>
#include <signal_source/client.h>

using namespace Genode;


class Signal_handler_thread : Thread, Blockade
{
	private:

		Pd_session  &_pd;
		Cpu_session &_cpu;

		/**
		 * Actual signal source
		 *
		 * Member must be constructed in the context of the signal handler
		 * thread because on some platforms (e.g., Fiasco.OC), the calling
		 * thread context is used for implementing the signal-source protocol.
		 */
		Constructible<Signal_source_client> _signal_source { };

		void entry() override
		{
			_signal_source.construct(_cpu, _pd.alloc_signal_source());
			wakeup();
			Signal_receiver::dispatch_signals(&(*_signal_source));
		}

		enum { STACK_SIZE = 4*1024*sizeof(addr_t) };

	public:

		/**
		 * Constructor
		 */
		Signal_handler_thread(Env &env)
		:
			Thread(env, "signal handler", STACK_SIZE), _pd(env.pd()), _cpu(env.cpu())
		{
			start();

			/*
			 * Make sure the signal source was constructed before proceeding
			 * with the use of signals. Otherwise, signals may get lost until
			 * the construction finished.
			 */
			block();
		}

		~Signal_handler_thread()
		{
			_pd.free_signal_source(_signal_source->rpc_cap());
		}
};


/*
 * The signal-handler thread will be constructed before global constructors are
 * called and, consequently, must not be a global static object. Otherwise, the
 * 'Constructible' constructor will be executed twice.
 */
static Constructible<Signal_handler_thread> & signal_handler_thread()
{
	return *unmanaged_singleton<Constructible<Signal_handler_thread> >();
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
}


/********************
 ** Signal_context **
 ********************/

void Signal_context::local_submit()
{
	if (_receiver) {
		Mutex::Guard guard(_mutex);
		/* construct and locally submit signal object */
		Signal::Data signal(this, 1);
		_receiver->local_submit(signal);
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
			Mutex mutable                       _mutex { };
			List<List_element<Signal_context> > _list { };

		public:

			void insert(List_element<Signal_context> *le)
			{
				Mutex::Guard guard(_mutex);
				_list.insert(le);
			}

			void remove(List_element<Signal_context> *le)
			{
				Mutex::Guard guard(_mutex);
				_list.remove(le);
			}

			bool test_and_lock(Signal_context *context) const
			{
				Mutex::Guard guard(_mutex);

				/* search list for context */
				List_element<Signal_context> const *le = _list.first();
				for ( ; le; le = le->next()) {

					if (context == le->object()) {
						/* acquire the object */
						context->_mutex.acquire();
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


/*********************
 ** Signal receiver **
 *********************/

static Pd_session *_pd_ptr;
static Parent     *_parent_ptr;


Signal_receiver::Signal_receiver() : _pd(*_pd_ptr)
{
	if (!_pd_ptr) {
		struct Missing_call_of_init_signal_receiver { };
		throw  Missing_call_of_init_signal_receiver();
	}
}


Signal_context_capability Signal_receiver::manage(Signal_context *context)
{
	if (context->_receiver)
		throw Context_already_in_use();

	context->_receiver = this;

	Mutex::Guard contexts_guard(_contexts_mutex);

	/* insert context into context list */
	_contexts.insert_as_tail(context);

	/* register context at process-wide registry */
	signal_context_registry()->insert(&context->_registry_le);

	for (;;) {

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		try {
			/* use signal context as imprint */
			context->_cap = _pd.alloc_context(_cap, (long)context);
			break;
		}
		catch (Out_of_ram)  { ram_upgrade = Ram_quota { 1024*sizeof(long) }; }
		catch (Out_of_caps) { cap_upgrade = Cap_quota { 4 }; }

		log("upgrading quota donation for PD session "
		    "(", ram_upgrade, " bytes, ", cap_upgrade, " caps)");

		_parent_ptr->upgrade(Parent::Env::pd(),
		                     String<100>("ram_quota=", ram_upgrade, ", "
		                                 "cap_quota=", cap_upgrade).string());
	}

	return context->_cap;
}


void Signal_receiver::block_for_signal()
{
	_signal_available.down();
}


Signal Signal_receiver::pending_signal()
{
	Mutex::Guard contexts_guard(_contexts_mutex);
	Signal::Data result;
	_contexts.for_each_locked([&] (Signal_context &context) -> bool {

		if (!context._pending) return false;

		_contexts.head(context._next);
		context._pending     = false;
		result               = context._curr_signal;
		context._curr_signal = Signal::Data();

		Trace::Signal_received trace_event(context, result.num);
		return true;
	});
	if (result.context) {
		Mutex::Guard context_guard(result.context->_mutex);
		if (result.num == 0)
			warning("returning signal with num == 0");

		return result;
	}

	/*
	 * Normally, we should never arrive at this point because that would
	 * mean, the '_signal_available' semaphore was increased without
	 * registering the signal in any context associated to the receiver.
	 *
	 * However, if a context gets dissolved right after submitting a
	 * signal, we may have increased the semaphore already. In this case
	 * the signal-causing context is absent from the list.
	 */
	return Signal();
}

void Signal_receiver::unblock_signal_waiter(Rpc_entrypoint &)
{
	_signal_available.up();
}


void Signal_receiver::local_submit(Signal::Data data)
{
	Signal_context *context = data.context;

	/*
	 * Replace current signal of the context by signal with accumulated
	 * counters. In the common case, the current signal is an invalid
	 * signal with a counter value of zero.
	 */
	unsigned num = context->_curr_signal.num + data.num;
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
			error("received null signal imprint, stop signal dispatcher");
			sleep_forever();
		}

		if (!signal_context_registry()->test_and_lock(context)) {
			warning("encountered dead signal context ", context, " in signal dispatcher");
			continue;
		}

		if (context->_receiver) {
			/* construct and locally submit signal object */
			Signal::Data signal(context, source_signal.num());
			context->_receiver->local_submit(signal);
		} else {
			warning("signal context ", context, " with no receiver in signal dispatcher");
		}

		/* free context mutex that was taken by 'test_and_lock' */
		context->_mutex.release();
	}
}


void Signal_receiver::_platform_begin_dissolve(Signal_context *context)
{
	/*
	 * Because the 'remove' operation takes the registry mutex, the context
	 * must not be acquired when calling this method. See the comment in
	 * 'Signal_receiver::dissolve'.
	 */
	signal_context_registry()->remove(&context->_registry_le);
}


void Signal_receiver::_platform_finish_dissolve(Signal_context *) { }


void Signal_receiver::_platform_destructor() { }


void Genode::init_signal_receiver(Pd_session &pd, Parent &parent)
{
	_pd_ptr     = &pd;
	_parent_ptr = &parent;
}
