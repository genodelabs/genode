/*
 * \brief  Signal context for IRQ's
 * \author Sebastian Sumpf <sebastian.sumpf@genode-labs.com>
 * \date   2012-05-23
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <signal.h>
#include <irq_session/client.h>

#include <extern_c_begin.h>
#include <lx_emul.h>
#include <extern_c_end.h>

#include <platform.h>


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
class Irq_context : public Genode::List<Irq_context>::Element
{
	private:

		typedef Genode::List<Irq_context>::Element LE;

		unsigned int                           _irq;          /* IRQ number */
		Genode::List<Irq_handler>              _handler_list; /* List of registered handlers */
		Genode::Signal_rpc_member<Irq_context> _dispatcher;
		Genode::Irq_session_capability         _irq_cap;
		Genode::Irq_session_client             _irq_client;

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

		/**
		 * Call one IRQ handler
		 */
		inline bool _handle_one(Irq_handler *h)
		{
			bool handled = false;

			/*
			 * It might be that the next interrupt triggers right after the
			 * device has acknowledged the IRQ. To reduce per-IRQ context
			 * switches, we merge up to 'MAX_MERGED_IRQS' calls to the
			 * interrupt handler.
			 */
			enum { MAX_MERGED_IRQS = 8 };
			for (unsigned i = 0; i < MAX_MERGED_IRQS; i++) {
				if (h->handler(_irq, h->dev) != IRQ_HANDLED)
					break;

				handled = true;
			}
			return handled;
		}

		/**
		 * Call all handlers registered for this context
		 */
		bool _handle()
		{
			bool handled = false;

			/* report IRQ to all clients */
			for (Irq_handler *h = _handler_list.first(); h; h = h->next())
				if ((handled = _handle_one(h))) {
					dde_kit_log(DEBUG_IRQ, "IRQ: %u ret: %u h: %p dev: %p", _irq, handled, h->handler, h->dev);
					break;
				}

			/* ack interrupt */
			_irq_client.ack_irq();

			if (handled)
				Routine::schedule_all();

			return handled;
		}

		void _handle(unsigned) { _handle(); }

	public:

		Irq_context(unsigned int irq)
		: _irq(irq),
		  _dispatcher(_signal->ep(), *this, &Irq_context::_handle),
		  _irq_cap(platform_irq_activate(_irq)),
		  _irq_client(_irq_cap)
		{
			if (!_irq_cap.valid()) {
				PERR("Interrupt %d attach failed", irq);
				return;
			}

			_irq_client.sigh(_dispatcher);
			_irq_client.ack_irq();

			_list()->insert(this);
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

		static bool check_irq()
		{
			bool handled = false;
			for (Irq_context *i = _list()->first(); i; i = i->LE::next())
				handled |= i->_handle();

			return handled;
		}
};


void Irq::init(Server::Entrypoint &ep) {
	_signal = new (Genode::env()->heap()) Signal_helper(ep); }


void Irq::check_irq()
{
	Irq_context::check_irq();
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

