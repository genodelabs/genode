/**
 * \brief  Shared-interrupt support
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2009-12-15
 */

#ifndef _CORE__INCLUDE__IRQ_PROXY_H_
#define _CORE__INCLUDE__IRQ_PROXY_H_

#include <base/env.h>
#include <util.h>
#include <base/thread.h>


namespace Genode {
	class Irq_blocker;
	template <typename THREAD> class Irq_proxy;
	class Irq_thread_dummy;
	class Irq_proxy_single;
}


class Genode::Irq_blocker : public Genode::List<Genode::Irq_blocker>::Element
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
template <typename THREAD>
class Genode::Irq_proxy : public THREAD,
                          public Genode::List<Genode::Irq_proxy<THREAD> >::Element
{
	protected:

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


		/***************
		 ** Interface **
		 ***************/

		/**
		 * Request interrupt
		 *
		 * \return  true on success
		 */
		virtual bool _associate() = 0;

		/**
		 * Wait for associated interrupt
		 */
		virtual void _wait_for_irq() = 0;

		/**
		 * Acknowledge interrupt
		 */
		virtual void _ack_irq() = 0;

		/********************
		 ** Implementation **
		 ********************/

		const char *_construct_name(long irq_number)
		{
			snprintf(_name, sizeof(_name), "irqproxy%02lx", irq_number);
			return _name;
		}

		void _loop()
		{
			/* wait for first blocker */
			_sleep.down();

			while (1) {
				_wait_for_irq();

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
				_ack_irq();
			}
		}

		/**
		 * Start this thread, should be called externally from derived class
		 */
		virtual void _start()
		{
			THREAD::start();
			_startup_lock.lock();
		}

	public:

		Irq_proxy(long irq_number)
		:
			THREAD(_construct_name(irq_number)),
			_startup_lock(Lock::LOCKED), _irq_number(irq_number),
			_mutex(Lock::UNLOCKED), _num_sharers(0), _num_blockers(0), _woken_up(false)
		{ }

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
		virtual void wait_for_irq()
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

		virtual bool add_sharer()
		{
			Lock::Guard lock_guard(_mutex);
			++_num_sharers;
			return true;
		}

		template <typename PROXY>
		static PROXY *get_irq_proxy(long irq_number, Range_allocator *irq_alloc = 0)
		{
			static List<Irq_proxy> proxies;
			static Lock            proxies_lock;

			Lock::Guard lock_guard(proxies_lock);

			/* lookup proxy in database */
			for (Irq_proxy *p = proxies.first(); p; p = p->next())
				if (p->irq_number() == irq_number)
					return static_cast<PROXY *>(p);

			/* try to create proxy */
			if (!irq_alloc || irq_alloc->alloc_addr(1, irq_number) != Range_allocator::ALLOC_OK)
				return false;

			PROXY *new_proxy = new (env()->heap()) PROXY(irq_number);
			proxies.insert(new_proxy);
			return new_proxy;
		}
};


/**
 * Dummy thread
 */
class Genode::Irq_thread_dummy
{
	public:

		Irq_thread_dummy(char const *name) { }
		void start() { }
};


/**
 * Non-threaded proxy that disables shared interrupts
 */
class Genode::Irq_proxy_single : public Genode::Irq_proxy<Genode::Irq_thread_dummy>
{
	protected:

		void _start()
		{
			_associate();
		}

	public:

		Irq_proxy_single(long irq_number) : Irq_proxy(irq_number) { }

		bool add_sharer()
		{
			Lock::Guard lock_guard(_mutex);

			if (_num_sharers)
				return false;

			_num_sharers = 1;
			return true;
		}

		void wait_for_irq()
		{
			_wait_for_irq();
			_ack_irq();
		}
};

#endif /* _CORE__INCLUDE__IRQ_PROXY_H_ */
