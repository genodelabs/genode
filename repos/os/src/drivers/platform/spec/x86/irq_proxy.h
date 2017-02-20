/**
 * \brief  Shared-interrupt support
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2009-12-15
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__PLATFORM__SPEC__X86__IRQ_PROXY_H_
#define _DRIVERS__PLATFORM__SPEC__X86__IRQ_PROXY_H_

namespace Platform {
	class Irq_sigh;
	class Irq_proxy;
}


class Platform::Irq_sigh : public Genode::Signal_context_capability,
                           public Genode::List<Platform::Irq_sigh>::Element
{
	public:

		inline Irq_sigh * operator= (const Genode::Signal_context_capability &cap)
		{
			Genode::Signal_context_capability::operator=(cap);
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
class Platform::Irq_proxy : public Genode::List<Platform::Irq_proxy>::Element
{
	protected:

		unsigned          _irq_number;

		Genode::Lock      _mutex;             /* protects this object */
		int               _num_sharers;       /* number of clients sharing this IRQ */
		Genode::List<Irq_sigh> _sigh_list;
		int               _num_acknowledgers; /* number of currently blocked clients */
		bool              _woken_up;          /* client decided to wake me up -
		                                         this prevents multiple wakeups
		                                         to happen during initialization */


	public:

		Irq_proxy(unsigned irq_number)
		:
			_irq_number(irq_number),
			_mutex(Genode::Lock::UNLOCKED), _num_sharers(0),
			_num_acknowledgers(0), _woken_up(false)

		{ }

		/**
		 * Acknowledgements of clients
		 */
		virtual bool ack_irq()
		{
			Genode::Lock::Guard lock_guard(_mutex);

			_num_acknowledgers++;

			/*
			 * The proxy thread has to be woken up if no client woke it up
			 * before and this client is the last aspired acknowledger.
			 */
			if (!_woken_up && _num_acknowledgers == _num_sharers) {
				_woken_up = true;
			}

			return _woken_up;
		}

		/**
		 * Notify all clients about irq
		 */
		void notify_about_irq()
		{
			Genode::Lock::Guard lock_guard(_mutex);

			/* reset acknowledger state */
			_num_acknowledgers = 0;
			_woken_up          = false;

			/* inform blocked clients */
			for (Irq_sigh * s = _sigh_list.first(); s ; s = s->next())
				s->notify();
		}

		unsigned irq_number() const { return _irq_number; }

		virtual bool add_sharer(Irq_sigh *s)
		{
			Genode::Lock::Guard lock_guard(_mutex);

			++_num_sharers;
			_sigh_list.insert(s);

			return true;
		}

		virtual bool remove_sharer(Irq_sigh *s)
		{
			Genode::Lock::Guard lock_guard(_mutex);

			_sigh_list.remove(s);
			--_num_sharers;

			if (_woken_up)
				return _num_sharers == 0;

			if (_num_acknowledgers == _num_sharers) {
				_woken_up = true;
			}

			return _num_sharers == 0;
		}
};

#endif /* _DRIVERS__PLATFORM__SPEC__X86__IRQ_PROXY_H_ */
