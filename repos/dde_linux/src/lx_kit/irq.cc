/*
 * \brief  Signal context for IRQ's
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/entrypoint.h>
#include <base/snprintf.h>
#include <base/tslab.h>
#include <irq_session/client.h>

/* Linux emulation environment */
#include <lx_emul.h>

/* Linux kit includes */
#include <lx_kit/irq.h>
#include <lx_kit/scheduler.h>


namespace Lx_kit { class Irq; }


class Lx_kit::Irq : public Lx::Irq
{
	private:

		/**
		 * Helper utilty for composing IRQ related names
		 */
		struct Name_composer
		{
			char name[16];

			Name_composer(Platform::Device &device)
			{
				Genode::snprintf(name, sizeof(name),
				                 "irq_%02x:%02x",
				                 device.vendor_id(),
				                 device.device_id());
			}
		};

		/**
		 * This contains the Linux-driver handlers
		 */
		class Handler : public Lx_kit::List<Handler>::Element
		{
			private:

				void          *_dev;       /* Linux device */
				irq_handler_t  _handler;   /* Linux handler */
				irq_handler_t  _thread_fn; /* Linux thread function */

			public:

				Handler(void *dev, irq_handler_t handler,
				        irq_handler_t thread_fn)
				: _dev(dev), _handler(handler), _thread_fn(thread_fn) { }

				bool handle()
				{
					switch (_handler(0, _dev)) {
					case IRQ_WAKE_THREAD:
						_thread_fn(0, _dev);
					case IRQ_HANDLED:
						return true;
					case IRQ_NONE:
						break;
					}
					return false;
				}
		};

	public:

		/**
		 * Context encapsulates the handling of an IRQ
		 */
		class Context : public Lx_kit::List<Context>::Element
		{
			private:

				Name_composer               _name;
				Platform::Device           &_dev;
				Genode::Irq_session_client  _irq_sess;
				Lx_kit::List<Handler>       _handler;
				Lx::Task                    _task;

				Genode::Signal_handler<Context> _dispatcher;

				static void _run_irq(void *args)
				{
					Context *ctx = static_cast<Context*>(args);

					while (1) {
						Lx::scheduler().current()->block_and_schedule();
						ctx->handle_irq();
					}
				}

			public:

				/**
				 * Constructor
				 */
				Context(Genode::Entrypoint &ep,
				        Platform::Device &dev)
				:
					_name(dev),
					_dev(dev),
					_irq_sess(dev.irq(0)),
					_task(_run_irq, this, _name.name, Lx::Task::PRIORITY_3, Lx::scheduler()),
					_dispatcher(ep, *this, &Context::unblock)
				{
					_irq_sess.sigh(_dispatcher);

					/* initial ack to receive further IRQ signals */
					_irq_sess.ack_irq();
				}

				/**
				 * Unblock this context, e.g., as result of an IRQ signal
				 */
				void unblock()
				{
					_task.unblock();

					/* kick off scheduling */
					Lx::scheduler().schedule();
				}

				/**
				 * Handle IRQ
				 */
				void handle_irq()
				{
					/* report IRQ to all clients */
					for (Handler *h = _handler.first(); h; h = h->next()) {
						h->handle();
					}

					_irq_sess.ack_irq();
				}

				/**
				 * Add linux handler to context
				 */
				void add_handler(Handler *h) { _handler.append(h); }

				bool device(Platform::Device &dev) {
					return (&dev == &_dev); }
		};

	private:

		using Context_slab = Genode::Tslab<Context, 3 * sizeof(Context)>;
		using Handler_slab = Genode::Tslab<Handler, 3 * sizeof(Handler)>;

		Genode::Entrypoint    &_ep;
		Lx_kit::List<Context>  _list;
		Context_slab           _context_alloc;
		Handler_slab           _handler_alloc;

		/**
		 * Find context for given device
		 */
		Context *_find_context(Platform::Device &dev)
		{
			for (Context *i = _list.first(); i; i = i->next())
				if (i->device(dev)) return i;
			return nullptr;
		}

		Irq(Genode::Entrypoint &ep, Genode::Allocator &alloc)
		: _ep(ep),
		  _context_alloc(&alloc),
		  _handler_alloc(&alloc) { }

	public:

		static Irq &irq(Genode::Entrypoint &ep, Genode::Allocator &alloc)
		{
			static Irq inst(ep, alloc);
			return inst;
		}

		/***********************
		 ** Lx::Irq interface **
		 ***********************/

		void request_irq(Platform::Device &dev, irq_handler_t handler,
		                 void *dev_id, irq_handler_t thread_fn = 0) override
		{
			Context *ctx = _find_context(dev);

			/* if this IRQ is not registered */
			if (!ctx) {
				ctx = new (&_context_alloc) Context(_ep, dev);
				_list.insert(ctx);
			}

			/* register Linux handler */
			Handler *h = new (&_handler_alloc)
			                   Handler(dev_id, handler, thread_fn);
			ctx->add_handler(h);
		}

		void inject_irq(Platform::Device &dev)
		{
			Context *ctx = _find_context(dev);
			if (ctx) ctx->unblock();
		}
};


/****************************
 ** Lx::Irq implementation **
 ****************************/

Lx::Irq &Lx::Irq::irq(Genode::Entrypoint *ep, Genode::Allocator *alloc) {
	return Lx_kit::Irq::irq(*ep, *alloc); }
