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
#include <dma.h>
extern "C" {
#include <dde_kit/interrupt.h>
}


/* our local incarnation of sender and receiver */
static Signal_helper *_signal = 0;


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
			for (Irq_context *i = _list()->first(); i; i = i->next())
				if (i->_irq == irq)
					return i;

			return 0;
		}

		/* called by the DDE kit upon IRQ */
		static void _dde_handler(void *irq)
		{
			Irq_context *ctx = static_cast<Irq_context *>(irq);

			/* set context & submit signal */
			_signal->sender()->context(ctx->_ctx_cap);
			_signal->sender()->submit();
		}

	public:

		Irq_context(unsigned int irq)
		: _irq(irq),
			_ctx_cap(_signal->receiver()->manage(this))
		{
			/* register at DDE (shared) */
			dde_kit_interrupt_attach(_irq, 1, 0, _dde_handler, this);
			dde_kit_interrupt_enable(_irq);
			_list()->insert(this);
		}

		void handle()
		{
			/* report IRQ to all clients */
			for (Irq_handler *h = _handler_list.first(); h; h = h->next()) {
				irqreturn_t rc;

				rc = h->handler(_irq, h->dev);
				dde_kit_log(DEBUG_IRQ, "IRQ: %u ret: %u %p", _irq, rc, h->handler);
				if (rc == IRQ_HANDLED) {
					Routine::schedule_all();
					return; 
				}
			}
		}

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
};


void Irq::init(Genode::Signal_receiver *recv) {
	_signal = new (Genode::env()->heap()) Signal_helper(recv); }


/***********************
 ** linux/interrupt.h **
 ***********************/

int request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
                 const char *name, void *dev)
{
	dde_kit_log(DEBUG_IRQ, "Request irq %u", irq);
	Irq_context::request_irq(irq, handler, dev);
	return 0;
}
