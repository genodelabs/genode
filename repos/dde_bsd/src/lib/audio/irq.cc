/*
 * \brief  Signal context for IRQ's
 * \author Josef Soentgen
 * \date   2014-10-14
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/thread.h>
#include <base/tslab.h>
#include <timer_session/connection.h>
#include <irq_session/connection.h>

/* local includes */
#include <audio/audio.h>
#include <bsd.h>
#include <extern_c_begin.h>
# include <bsd_emul.h>
#include <extern_c_end.h>


namespace Bsd {
	class Irq;
}

static void run_irq(void *args);

static Genode::Signal_context_capability _dma_notifier_cap;

void Audio::dma_notifier(Genode::Signal_context_capability cap) {
	_dma_notifier_cap = cap; }

class Bsd::Irq
{
	public:

		typedef int (*intrh_t)(void*);

		/**
		 * Context encapsulates the handling of an IRQ
		 */
		class Context
		{
			private:

				enum { STACK_SIZE = 1024 * sizeof(long) };

				Bsd::Task                          _task;
				Genode::Irq_session_client         _irq;
				Genode::Signal_rpc_member<Context> _dispatcher;

				intrh_t  _intrh;
				void    *_intarg;

				/**
				 * Signal handler
				 */
				void _handle(unsigned)
				{
					_task.unblock();
					Bsd::scheduler().schedule();
				}

			public:

				/**
				 * Constructor
				 */
				Context(Server::Entrypoint &ep,
				        Genode::Irq_session_capability cap,
				        intrh_t intrh, void *intarg)
				:
					_task(run_irq, this, "irq", Bsd::Task::PRIORITY_3,
					      Bsd::scheduler(), STACK_SIZE),
					_irq(cap),
					_dispatcher(ep, *this, &Context::_handle),
					_intrh(intrh), _intarg(intarg)
				{
					_irq.sigh(_dispatcher);
					_irq.ack_irq();
				}

				/**
				 * Handle IRQ
				 */
				void handle_irq()
				{
					_intrh(_intarg);
					_irq.ack_irq();

					/*
					 * Notify the frontend when a block from the DMA
					 * was played.
					 */
					if (_dma_notifier_cap.valid())
						Genode::Signal_transmitter(_dma_notifier_cap).submit();
				}
		};

	private:

		Genode::Allocator  &_alloc;
		Server::Entrypoint &_ep;
		Context            *_ctx;

	public:

		/**
		 * Constructor
		 */
		Irq(Genode::Allocator &alloc, Server::Entrypoint &ep)
		: _alloc(alloc), _ep(ep), _ctx(nullptr) { }

		/**
		 * Request an IRQ
		 */
		void establish_intr(Genode::Irq_session_capability cap, intrh_t intrh, void *intarg)
		{
			if (_ctx) {
				PERR("interrupt already established");
				Genode::sleep_forever();
			}

			_ctx = new (&_alloc) Context(_ep, cap, intrh, intarg);
		}
};


static Bsd::Irq *_bsd_irq;


void Bsd::irq_init(Server::Entrypoint &ep)
{
	static Bsd::Irq irq_context(*Genode::env()->heap(), ep);
	_bsd_irq = &irq_context;
}


static void run_irq(void *args)
{
	Bsd::Irq::Context *ctx = static_cast<Bsd::Irq::Context*>(args);

	while (true) {
		Bsd::scheduler().current()->block_and_schedule();
		ctx->handle_irq();
	}
}


/**********************
 ** dev/pci/pcivar.h **
 **********************/

extern "C" int pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ih) {
	return 0; }


extern "C" void *pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih,
                                    int ipl, int (*intrh)(void *), void *intarg,
                                    const char *intrstr)
{
	Bsd::Bus_driver *drv = (Bsd::Bus_driver*)pc;

	_bsd_irq->establish_intr(drv->irq_session(), intrh, intarg);
	return _bsd_irq;
}
