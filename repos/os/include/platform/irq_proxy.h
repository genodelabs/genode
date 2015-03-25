/**
 * \brief  Shared-interrupt support
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2009-12-15
 */

#ifndef _CORE__INCLUDE__IRQ_PROXY_H_
#define _CORE__INCLUDE__IRQ_PROXY_H_

#include <base/env.h>


namespace Genode {
	class Irq_sigh;
	template <typename THREAD> class Irq_proxy;
}


class Genode::Irq_sigh : public Genode::Signal_context_capability,
                         public Genode::List<Genode::Irq_sigh>::Element
{
	public:

		inline Irq_sigh * operator= (const Signal_context_capability &cap)
		{
			Signal_context_capability::operator=(cap);
			return this;
		}

		Irq_sigh() { }

		void notify() { Genode::Signal_transmitter(*this).submit(1); }
};


/*
 * Proxy thread that associates to the interrupt and unblocks waiting irqctrl
 * threads.
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
		List<Irq_sigh>    _sigh_list;
		int               _num_acknowledgers; /* number of currently blocked clients */
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

				/* notify all */
				notify_about_irq(1);

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
			_mutex(Lock::UNLOCKED), _num_sharers(0), _num_acknowledgers(0), _woken_up(false)

		{ }

		/**
		 * Thread interface
		 */
		void entry()
		{
			bool const associate_suceeded = _associate();

			_startup_lock.unlock();

			if (associate_suceeded)
				_loop();
		}

		/**
		 * Acknowledgements of clients
		 */
		virtual bool ack_irq()
		{
			Lock::Guard lock_guard(_mutex);

			_num_acknowledgers++;

			/*
			 * The proxy thread has to be woken up if no client woke it up
			 * before and this client is the last aspired acknowledger.
			 */
			if (!_woken_up && _num_acknowledgers == _num_sharers) {
				_sleep.up();
				_woken_up = true;
			}

			return _woken_up;
		}

		/**
		 * Notify all clients about irq
		 */
		void notify_about_irq(unsigned)
		{
			Lock::Guard lock_guard(_mutex);

			/* reset acknowledger state */
			_num_acknowledgers = 0;
			_woken_up          = false;

			/* inform blocked clients */
			for (Irq_sigh * s = _sigh_list.first(); s ; s = s->next())
				s->notify();
		}

		long irq_number() const { return _irq_number; }

		virtual bool add_sharer(Irq_sigh *s)
		{
			Lock::Guard lock_guard(_mutex);

			++_num_sharers;
			_sigh_list.insert(s);

			return true;
		}

		virtual bool remove_sharer(Irq_sigh *s)
		{
			Lock::Guard lock_guard(_mutex);

			_sigh_list.remove(s);
			--_num_sharers;

			if (_woken_up)
				return _num_sharers == 0;

			if (_num_acknowledgers == _num_sharers) {
				_sleep.up();
				_woken_up = true;
			}

			return _num_sharers == 0;
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
			if (!irq_alloc || irq_alloc->alloc_addr(1, irq_number).is_error())
				return 0;

			PROXY *new_proxy = new (env()->heap()) PROXY(irq_number);
			proxies.insert(new_proxy);
			return new_proxy;
		}
};

#endif /* _CORE__INCLUDE__IRQ_PROXY_H_ */
