/*
 * \brief  OKL4-specific implementation of IRQ sessions
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2009-12-15
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <util.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/interrupt.h>
#include <l4/security.h>
#include <l4/ipc.h>
} }

using namespace Genode;
using namespace Okl4;


/* XXX move this functionality to a central place instead of duplicating it */
static inline Okl4::L4_ThreadId_t thread_get_my_global_id()
{
	Okl4::L4_ThreadId_t myself;
	myself.raw = Okl4::__L4_TCR_ThreadWord(UTCB_TCR_THREAD_WORD_MYSELF);
	return myself;
}


/******************************
 ** Shared-interrupt support **
 ******************************/

class Irq_blocker : public List<Irq_blocker>::Element
{
	private:

		Lock _wait_lock;

	public:

		Irq_blocker() : _wait_lock(Lock::LOCKED) { }

		void block()   { _wait_lock.lock(); }
		void unblock() { _wait_lock.unlock(); }
};


/*
 * Proxy thread that associates to the interrupt and unblocks waiting irqctrl
 * threads. Maybe, we should utilize our signals for interrupt delivery...
 *
 * XXX resources are not accounted as the interrupt is shared
 */
class Irq_proxy : public Thread<0x1000>,
                  public List<Irq_proxy>::Element
{
	private:

		char              _name[32];
		Lock              _startup_lock;

		long              _irq_number;

		Lock              _mutex;             /* protects this object */
		int               _num_sharers;       /* number of clients sharing this IRQ */
		Semaphore         _sleep;             /* wake me up if aspired blockers return */
		List<Irq_blocker> _blocker_list;
		int               _num_blockers;      /* number of currently blocked clients */
		bool              _woken_up;          /* client decided to wake me up -
		                                         this prevents multiple wakeups
		                                         to happen during initialization */

		const char *_construct_name(long irq_number)
		{
			snprintf(_name, sizeof(_name), "irqproxy%02lx", irq_number);
			return _name;
		}

		bool _associate()
		{
			/* allow roottask (ourself) to handle the interrupt */
			L4_LoadMR(0, _irq_number);
			int ret = L4_AllowInterruptControl(L4_rootspace);
			if (ret != 1) {
				PERR("L4_AllowInterruptControl returned %d, error code=%ld\n",
				     ret, L4_ErrorCode());
				return false;
			}

			/* bit to use for IRQ notifications */
			enum { IRQ_NOTIFY_BIT = 13 };

			/*
			 * Note: 'L4_Myself()' does not work for the thread argument of
			 *       'L4_RegisterInterrupt'. We have to specify our global ID.
			 */
			L4_LoadMR(0, _irq_number);
			ret = L4_RegisterInterrupt(thread_get_my_global_id(), IRQ_NOTIFY_BIT, 0, 0);
			if (ret != 1) {
				PERR("L4_RegisterInterrupt returned %d, error code=%ld\n",
				     ret, L4_ErrorCode());
				return false;
			}

			/* prepare ourself to receive asynchronous IRQ notifications */
			L4_Set_NotifyMask(1 << IRQ_NOTIFY_BIT);
			L4_Accept(L4_NotifyMsgAcceptor);

			return true;
		}

		void _loop()
		{
			/* wait for first blocker */
			_sleep.down();

			while (1) {
				/* wait for asynchronous interrupt notification */
				L4_ThreadId_t partner = L4_nilthread;
				L4_ReplyWait(partner, &partner);

				{
					Lock::Guard lock_guard(_mutex);

					/* inform blocked clients */
					Irq_blocker *b;
					while ((b = _blocker_list.first())) {
						_blocker_list.remove(b);
						b->unblock();
					}

					/* reset blocker state */
					_num_blockers = 0;
					_woken_up     = false;
				}

				/*
				 * We must wait for all clients to ack their interrupt,
				 * otherwise level-triggered interrupts will occur immediately
				 * after acknowledgement. That's an inherent security problem
				 * with shared IRQs and induces problems with dynamic driver
				 * load and unload.
				 */
				_sleep.down();

				/* acknowledge previous interrupt */
				L4_LoadMR(0, _irq_number);
				L4_AcknowledgeInterrupt(0, 0);
			}
		}

	public:

		Irq_proxy(long irq_number)
		:
			Thread<0x1000>(_construct_name(irq_number)),
			_startup_lock(Lock::LOCKED), _irq_number(irq_number),
			_mutex(Lock::UNLOCKED), _num_sharers(0), _num_blockers(0), _woken_up(false)
		{
			start();
			_startup_lock.lock();
		}

		/**
		 * Thread interface
		 */
		void entry()
		{
			if (_associate()) {
				_startup_lock.unlock();
				_loop();
			}
		}

		/**
		 * Block until interrupt occured
		 */
		void wait_for_irq()
		{
			Irq_blocker blocker;
			{
				Lock::Guard lock_guard(_mutex);

				_blocker_list.insert(&blocker);
				_num_blockers++;

				/*
				 * The proxy thread is woken up if no client woke it up before
				 * and this client is the last aspired blocker.
				 */
				if (!_woken_up && _num_blockers == _num_sharers) {
					_sleep.up();
					_woken_up = true;
				}
			}
			blocker.block();
		}

		long irq_number() const { return _irq_number; }

		void add_sharer()
		{
			Lock::Guard lock_guard(_mutex);
			++_num_sharers;
		}
};


static Irq_proxy *get_irq_proxy(long irq_number, Range_allocator *irq_alloc = 0)
{
	static List<Irq_proxy> proxies;
	static Lock            proxies_lock;

	Lock::Guard lock_guard(proxies_lock);

	/* lookup proxy in database */
	for (Irq_proxy *p = proxies.first(); p; p = p->next())
		if (p->irq_number() == irq_number)
			return p;

	/* try to create proxy */
	if (!irq_alloc || irq_alloc->alloc_addr(1, irq_number) != Range_allocator::ALLOC_OK)
		return 0;

	Irq_proxy *new_proxy = new (env()->heap()) Irq_proxy(irq_number);
	proxies.insert(new_proxy);

	return new_proxy;
}


/***************************
 ** IRQ session component **
 ***************************/

bool Irq_session_component::Irq_control_component::associate_to_irq(unsigned irq)
{
	return true;
}


void Irq_session_component::wait_for_irq()
{
	/* block at interrupt proxy */
	Irq_proxy *p = get_irq_proxy(_irq_number);
	if (!p) {
		PERR("Expected to find IRQ proxy for IRQ %02x", _irq_number);
		return;
	}

	p->wait_for_irq();

	/* interrupt ocurred and proxy woke us up */
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_alloc(irq_alloc),
	_ep(cap_session, STACK_SIZE, "irqctrl"),
	_irq_attached(false),
	_control_client(Capability<Irq_session_component::Irq_control>())
{
	/*
	 * XXX Removed irq_shared argument as this is the default now. If we need
	 * exclusive later on, we should add this as new argument.
	 */

	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1) {
		PERR("invalid IRQ number requested");

		throw Root::Unavailable();
	}

	/* check if IRQ thread was started before */
	Irq_proxy *irq_proxy = get_irq_proxy(irq_number, irq_alloc);
	if (!irq_proxy) {
		PERR("unavailable IRQ %lx requested", irq_number);

		throw Root::Unavailable();
	}

	irq_proxy->add_sharer();
	_irq_number = irq_number;

	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


Irq_session_component::~Irq_session_component()
{
	PERR("not yet implemented");
	/* TODO del_sharer() resp. put_sharer() */
}

