/*
 * \brief  Signal context for IRQ's
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <signal.h>
#include <lx_emul.h>

extern "C" {
#include <dde_kit/interrupt.h>
}

/* our local incarnation of sender and receiver */
static Signal_helper *_signal = 0;
static Genode::Lock   _irq_sync(Genode::Lock::LOCKED);
static Genode::Lock   _irq_wait(Genode::Lock::LOCKED);


/**
 * This contains the Linux-driver handlers
 */
struct Irq_handler : Genode::List<Irq_handler>::Element
{
	void         *dev;     /* Linux device */
	irq_handler_t handler; /* Linux handler */

	Irq_handler(void *dev, irq_handler_t handler)
	: dev(dev), handler(handler) { }
};



/**
 * Signal context for IRQs
 */
class Irq_context : public Driver_context,
                    public Genode::List<Irq_context>::Element
{
	private:

		typedef Genode::List<Irq_context>::Element LE;

		unsigned int                      _irq;          /* IRQ number */
		Genode::List<Irq_handler>         _handler_list; /* List of registered handlers */
		Genode::Signal_context_capability _ctx_cap;      /* capability for this context */

		static Genode::List<Irq_context> *_list()
		{
			static Genode::List<Irq_context> _l;
			return &_l;
		}

		/**
		 * Find context for given IRQ number
		 */
		static Irq_context *_find_ctx(unsigned int irq)
		{

			for (Irq_context *i = _list()->first(); i; i = i->LE::next())
				if (i->_irq == irq)
					return i;

			return 0;
		}

		/* called by the DDE kit upon IRQ */
		static void _dde_handler(void *irq)
		{
			/*
			 * Make sure there is only one interrupt handled at a time, since dde_kit
			 * will use one thread per IRQ
			 */
			static Genode::Lock handler_lock;
			Genode::Lock::Guard guard(handler_lock);

			/* unlock if main thread is waiting */
			_irq_wait.unlock();

			Irq_context *ctx = static_cast<Irq_context *>(irq);

			/* set context & submit signal */
			_signal->sender()->context(ctx->_ctx_cap);
			_signal->sender()->submit();

			/* wait for interrupt to get acked at device side */
			_irq_sync.lock();
		}

		/**
		 * Call one IRQ handler
		 */
		inline bool _handle_one(Irq_handler *h)
		{
			bool handled = false;

			/*
			 * It might be that the next interrupt triggers right after the device has
			 * acknowledged the IRQ
			 */
			do {
				if (h->handler(_irq, h->dev) != IRQ_HANDLED)
					return handled;

				handled = true;

			} while (true);
		}

		/**
		 * Call all handlers registered for this context
		 */
		bool _handle()
		{
			bool handled = false;

			/* report IRQ to all clients */
			for (Irq_handler *h = _handler_list.first(); h; h = h->next()) {

				handled |= _handle_one(h);
				dde_kit_log(DEBUG_IRQ, "IRQ: %u ret: %u h: %p dev: %p", _irq, handled, h->handler, h->dev);
			}

			/* interrupt should be acked at device now */
			_irq_sync.unlock();

			if (handled)
				Routine::schedule_all();

			return handled;
		}

	public:

		Irq_context(unsigned int irq)
		: _irq(irq),
			_ctx_cap(_signal->receiver()->manage(this))
		{
			/* register at DDE (shared) */
			int ret = dde_kit_interrupt_attach(_irq, 0, 0, _dde_handler, this);
			if (ret)
				PERR("Interrupt attach return %d for IRQ %u", ret, irq);

			dde_kit_interrupt_enable(_irq);
			_list()->insert(this);
		}


		inline void handle() { _handle(); }
		const char *debug() { return "Irq_context"; }

		/**
		 * Request an IRQ
		 */
		static void request_irq(unsigned int irq, irq_handler_t handler, void *dev)
		{
			Irq_handler *h   = new(Genode::env()->heap()) Irq_handler(dev, handler);
			Irq_context *ctx = _find_ctx(irq);

			/* if this IRQ is not registered */
			if (!ctx)
				ctx = new (Genode::env()->heap()) Irq_context(irq);

			/* register Linux handler */
			ctx->_handler_list.insert(h);
		}

		static bool check_irq()
		{
			bool handled = false;
			for (Irq_context *i = _list()->first(); i; i = i->LE::next())
				handled |= i->_handle();

			return handled;
		}

		static void wait()
		{
			_irq_wait.lock();
			check_irq();
		}
};


void Irq::init(Genode::Signal_receiver *recv) {
	_signal = new (Genode::env()->heap()) Signal_helper(recv); }


void Irq::check_irq()
{
	if (!Irq_context::check_irq())
		Irq_context::wait();
}


/***********************
 ** linux/interrupt.h **
 ***********************/

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                 const char *name, void *dev)
{
	dde_kit_log(DEBUG_IRQ, "Request irq %u handler %p", irq, handler);
	Irq_context::request_irq(irq, handler, dev);
	return 0;
}

