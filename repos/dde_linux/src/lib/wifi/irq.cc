/*
 * \brief  Signal context for IRQ's
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/tslab.h>
#include <timer_session/connection.h>
#include <irq_session/connection.h>
#include <pci_device/client.h>

/* local includes */
#include <lx.h>

#include <extern_c_begin.h>
# include <lx_emul.h>
#include <extern_c_end.h>

namespace {

	/**
	 * Helper utilty for composing IRQ related names
	 */
	struct Name_composer
	{
		char name[16];
		Name_composer(unsigned irq) {
			Genode::snprintf(name, sizeof(name), "irq_%02x", irq); }
	};
}

/**
 * Task for interrupts
 *
 * Allows flagging of IRQs from other threads.
 */
struct Irq_task
{
	private:

		Lx::Task      _task;

	public:

		Irq_task(void (*func)(void *), void *args, char const *name)
		: _task(func, args, name, Lx::Task::PRIORITY_3, Lx::scheduler()) { }

		void unblock() { _task.unblock(); }

		char const *name() { return _task.name(); }
};


/**
 * This contains the Linux-driver handlers
 */
struct Lx_irq_handler : public Lx::List<Lx_irq_handler>::Element
{
	void          *dev;       /* Linux device */
	irq_handler_t  handler;   /* Linux handler */
	irq_handler_t  thread_fn; /* Linux thread function */

	Lx_irq_handler(void *dev, irq_handler_t handler, irq_handler_t thread_fn)
	: dev(dev), handler(handler), thread_fn(thread_fn) { }
};


namespace Lx {
	class Irq;
}

static void run_irq(void *args);

extern "C" Pci::Device_capability pci_device_cap;

/**
 * Lx::Irq
 */
class Lx::Irq
{
	public:

		/**
		 * Context encapsulates the handling of an IRQ
		 */
		class Context : public Lx::List<Context>::Element
		{
			private:

				Name_composer              _name;

				unsigned int               _irq;     /* IRQ number */
				Genode::Irq_session_client _irq_sess;
				Lx::List<Lx_irq_handler>   _handler; /* List of registered handlers */
				Irq_task                   _task;

				Genode::Signal_transmitter         _sender;
				Genode::Signal_rpc_member<Context> _dispatcher;

				/**
				 * Call one IRQ handler
				 */
				bool _handle_one(Lx_irq_handler *h)
				{
					bool handled = false;

					/* XXX the handler as well as the thread_fn are called in a synchronouse fashion */
					switch (h->handler(_irq, h->dev)) {
					case IRQ_WAKE_THREAD:
					{
						h->thread_fn(_irq, h->dev);
					}
					case IRQ_HANDLED:
						handled = true;
						break;
					case IRQ_NONE:
						break;
					}

					return handled;
				}

				/**
				 * Signal handler
				 */
				void _handle(unsigned)
				{
					_task.unblock();

					/* kick off scheduling */
					Lx::scheduler().schedule();
				}

			public:

				/**
				 * Constructor
				 */
				Context(Server::Entrypoint &ep, unsigned irq,
				        Pci::Device_capability pci_dev)
				:
					_name(irq),
					_irq(irq),
					_irq_sess(Pci::Device_client(pci_dev).irq(0)),
					_task(run_irq, this, _name.name),
					_dispatcher(ep, *this, &Context::_handle)
				{
					_irq_sess.sigh(_dispatcher);

					/* initial ack to receive further IRQ signals */
					_irq_sess.ack_irq();
				}

				/**
				 * Return IRQ number
				 */
				unsigned irq() { return _irq; }

				/**
				 * Handle IRQ
				 */
				void handle_irq()
				{
					bool handled = false;

					/* report IRQ to all clients */
					for (Lx_irq_handler *h = _handler.first(); h; h = h->next()) {
						if ((handled = _handle_one(h)))
							break;
					}

					_irq_sess.ack_irq();
				}

				/**
				 * Add linux handler to context
				 */
				void add_handler(Lx_irq_handler *h) { _handler.append(h); }
		};

	private:

		Server::Entrypoint    &_ep;
		Lx::List<Context>      _list;

		Genode::Tslab<Context,
		              3 * sizeof(Context)>        _context_alloc;
		Genode::Tslab<Lx_irq_handler,
		              3 * sizeof(Lx_irq_handler)> _handler_alloc;

		/**
		 * Find context for given IRQ number
		 */
		Context *_find_context(unsigned int irq)
		{
			for (Context *i = _list.first(); i; i = i->next())
				if (i->irq() == irq)
					return i;

			return 0;
		}

	public:

		/**
		 * Constructor
		 */
		Irq(Server::Entrypoint &ep)
		:
			_ep(ep),
			_context_alloc(Genode::env()->heap()),
			_handler_alloc(Genode::env()->heap())
		{ }

		/**
		 * Request an IRQ
		 */
		void request_irq(unsigned int irq, irq_handler_t handler, void *dev,
		                 irq_handler_t thread_fn = 0)
		{
			Context *ctx = _find_context(irq);

			/* if this IRQ is not registered */
			if (!ctx)
				ctx = new (&_context_alloc) Context(_ep, irq, pci_device_cap);

			/* register Linux handler */
			Lx_irq_handler *h = new (&_handler_alloc)
			                         Lx_irq_handler(dev, handler, thread_fn);
			ctx->add_handler(h);
		}
};


static Lx::Irq *_lx_irq;


void Lx::irq_init(Server::Entrypoint &ep)
{
	static Lx::Irq irq_context(ep);
	_lx_irq = &irq_context;
}


static void run_irq(void *args)
{
	Lx::Irq::Context *ctx = static_cast<Lx::Irq::Context*>(args);

	while (1) {
		Lx::scheduler().current()->block_and_schedule();

		ctx->handle_irq();
	}
}


extern "C" {

/***********************
 ** linux/interrupt.h **
 ***********************/

int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev)
{
	_lx_irq->request_irq(irq, handler, dev);
	return 0;
}


int request_threaded_irq(unsigned int irq, irq_handler_t handler,
                         irq_handler_t thread_fn,
                         unsigned long flags, const char *name,
                         void *dev)
{
	_lx_irq->request_irq(irq, handler, dev, thread_fn);
	return 0;
}

} /* extern "C" */
