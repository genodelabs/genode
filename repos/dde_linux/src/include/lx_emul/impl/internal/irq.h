/*
 * \brief  Signal context for IRQ's
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_EMUL__IMPL__INTERNAL__IRQ_H_
#define _LX_EMUL__IMPL__INTERNAL__IRQ_H_

/* Genode includes */
#include <base/tslab.h>
#include <irq_session/client.h>
#include <platform_device/client.h>

/* Linux emulation environment includes */
#include <lx_emul/impl/internal/task.h>

namespace Lx { class Irq; }


class Lx::Irq
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
		class Handler : public Lx::List<Handler>::Element
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
		class Context : public Lx::List<Context>::Element
		{
			private:

				Name_composer              _name;
				Platform::Device   &_dev;
				Genode::Irq_session_client _irq_sess;
				Lx::List<Handler>   _handler; /* List of registered handlers */
				Lx::Task                   _task;

				Genode::Signal_rpc_member<Context> _dispatcher;

				/**
				 * Signal handler
				 */
				void _handle(unsigned)
				{
					_task.unblock();

					/* kick off scheduling */
					Lx::scheduler().schedule();
				}

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
				Context(Server::Entrypoint &ep,
				        Platform::Device &dev)
				:
					_name(dev),
					_dev(dev),
					_irq_sess(dev.irq(0)),
					_task(_run_irq, this, _name.name, Lx::Task::PRIORITY_3, Lx::scheduler()),
					_dispatcher(ep, *this, &Context::_handle)
				{
					_irq_sess.sigh(_dispatcher);

					/* initial ack to receive further IRQ signals */
					_irq_sess.ack_irq();
				}

				/**
				 * Handle IRQ
				 */
				void handle_irq()
				{
					bool handled = false;

					/* report IRQ to all clients */
					for (Handler *h = _handler.first(); h; h = h->next()) {
						if ((handled = h->handle())) break;
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

		Server::Entrypoint &_ep;
		Lx::List<Context>   _list;
		Context_slab        _context_alloc;
		Handler_slab        _handler_alloc;

		/**
		 * Find context for given device
		 */
		Context *_find_context(Platform::Device &dev)
		{
			for (Context *i = _list.first(); i; i = i->next())
				if (i->device(dev)) return i;
			return nullptr;
		}

		Irq(Server::Entrypoint &ep)
		: _ep(ep),
		  _context_alloc(Genode::env()->heap()),
		  _handler_alloc(Genode::env()->heap()) { }

	public:

		static Irq &irq(Server::Entrypoint *ep = nullptr);

		/**
		 * Request an IRQ
		 */
		void request_irq(Platform::Device &dev, irq_handler_t handler,
		                 void *dev_id, irq_handler_t thread_fn = 0)
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
};

#endif /* _LX_EMUL__IMPL__INTERNAL__IRQ_H_ */
