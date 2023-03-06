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
#include <base/tslab.h>
#include <irq_session/client.h>

/* format-string includes */
#include <format/snprintf.h>

/* Linux emulation environment */
#include <lx_emul.h>

/* Linux kit includes */
#include <legacy/lx_kit/irq.h>
#include <legacy/lx_kit/scheduler.h>


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

			Name_composer(unsigned number)
			{
				Format::snprintf(name, sizeof(name),
				                 "irq_%02x", number);
			}
		};

		/**
		 * This contains the Linux-driver handlers
		 */
		class Handler : public Lx_kit::List<Handler>::Element
		{
			private:

				void          *_dev;       /* Linux device */
				unsigned int   _irq;       /* Linux IRQ number */
				irq_handler_t  _handler;   /* Linux handler */
				irq_handler_t  _thread_fn; /* Linux thread function */

			public:

				Handler(void *dev, unsigned int irq, irq_handler_t handler,
				        irq_handler_t thread_fn)
				: _dev(dev), _irq(irq), _handler(handler),
				  _thread_fn(thread_fn) { }

				bool handle()
				{
					if (!_handler) {
						/* on Linux, having no handler implies IRQ_WAKE_THREAD */
						_thread_fn(_irq, _dev);
						return true;
					}

					switch (_handler(_irq, _dev)) {
					case IRQ_WAKE_THREAD:
						_thread_fn(_irq, _dev);
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
				unsigned int                _irq;
				Genode::Irq_session_client  _irq_sess;
				Lx_kit::List<Handler>       _handler;
				Lx::Task                    _task;
				bool                        _irq_enabled;
				bool                        _irq_ack_pending;

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
				        Genode::Irq_session_capability cap,
				        unsigned int irq)
				:
					_name(irq),
					_irq(irq),
					_irq_sess(cap),
					_task(_run_irq, this, _name.name, Lx::Task::PRIORITY_3, Lx::scheduler()),
					_irq_enabled(true),
					_irq_ack_pending(false),
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
					if (_irq_enabled) {

						/* report IRQ to all clients */
						for (Handler *h = _handler.first(); h; h = h->next())
							h->handle();

						_irq_sess.ack_irq();

					} else {

						/*
						 * IRQs are disabled by not acknowledging, so one IRQ
						 * can still occur in the 'disabled' state. It must be
						 * acknowledged later by 'enable_irq()'.
						 */

						_irq_ack_pending = true;
					}
				}

				/**
				 * Add linux handler to context
				 */
				void add_handler(Handler *h) { _handler.append(h); }

				bool irq(unsigned int irq) {
					return (irq == _irq); }

				void disable_irq()
				{
					_irq_enabled = false;
				}

				void enable_irq()
				{
					if (_irq_enabled)
						return;

					if (_irq_ack_pending) {
						_irq_sess.ack_irq();
						_irq_ack_pending = false;
					}

					_irq_enabled = true;
				}
		};

	private:

		using Context_slab = Genode::Tslab<Context, 3 * sizeof(Context)>;
		using Handler_slab = Genode::Tslab<Handler, 3 * sizeof(Handler)>;

		Genode::Entrypoint    &_ep;
		Lx_kit::List<Context>  _list;
		Context_slab           _context_alloc;
		Handler_slab           _handler_alloc;

		/**
		 * Find context for given IRQ number
		 */
		Context *_find_context(unsigned int irq)
		{
			for (Context *i = _list.first(); i; i = i->next())
				if (i->irq(irq)) return i;
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

		void request_irq(Genode::Irq_session_capability cap,
		                 unsigned int                   irq,
		                 irq_handler_t                  handler,
		                 void                         * dev_id,
		                 irq_handler_t                  thread_fn = 0) override
		{
			Context *ctx = _find_context(irq);

			/* if this IRQ is not registered */
			if (!ctx) {
				ctx = new (&_context_alloc) Context(_ep, cap, irq);
				_list.insert(ctx);
			}

			/* register Linux handler */
			Handler *h = new (&_handler_alloc)
			                   Handler(dev_id, irq, handler, thread_fn);
			ctx->add_handler(h);
		}

		void inject_irq(unsigned int irq)
		{
			Context *ctx = _find_context(irq);
			if (ctx) ctx->unblock();
		}

		void disable_irq(unsigned int irq)
		{
			Context *ctx = _find_context(irq);
			if (ctx) ctx->disable_irq();
		}

		void enable_irq(unsigned int irq)
		{
			Context *ctx = _find_context(irq);
			if (ctx) ctx->enable_irq();
		}
};


/****************************
 ** Lx::Irq implementation **
 ****************************/

Lx::Irq &Lx::Irq::irq(Genode::Entrypoint *ep, Genode::Allocator *alloc) {
	return Lx_kit::Irq::irq(*ep, *alloc); }
