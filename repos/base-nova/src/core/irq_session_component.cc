/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <irq_root.h>
#include <irq_proxy.h>
#include <platform.h>
#include <platform_pd.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>
#include <nova_util.h>

using namespace Genode;

namespace Genode {
	class Irq_proxy_component;
}


/**
 * Global worker (i.e. thread with SC)
 */
class Irq_thread : public Thread_base
{
	private:

		enum { STACK_SIZE = 1024 * sizeof(addr_t) };
		enum { WEIGHT = Cpu_session::DEFAULT_WEIGHT };

		static void _thread_start()
		{
			Thread_base::myself()->entry();
			sleep_forever();
		}

	public:

		Irq_thread(char const *name) : Thread_base(WEIGHT, name, STACK_SIZE)
		{ }

		/**
		 * Create global EC, associate it to SC
		 */
		void start()
		{
			using namespace Nova;
			addr_t pd_sel   = Platform_pd::pd_core_sel();
			addr_t utcb = reinterpret_cast<addr_t>(&_context->utcb);

			/*
			 * Put IP on stack, it will be read from core pager in platform.cc
			 */
			addr_t *sp = reinterpret_cast<addr_t *>(_context->stack_top() - sizeof(addr_t));
			*sp = reinterpret_cast<addr_t>(_thread_start);

			/* create global EC */
			enum { GLOBAL = true };
			uint8_t res = create_ec(_tid.ec_sel, pd_sel, boot_cpu(),
			                        utcb, (mword_t)sp, _tid.exc_pt_sel, GLOBAL);
			if (res != NOVA_OK) {
				PERR("%p - create_ec returned %d", this, res);
				throw Cpu_session::Thread_creation_failed();
			}

			/* remap startup portal from main thread */
			if (map_local((Utcb *)Thread_base::myself()->utcb(),
			              Obj_crd(PT_SEL_STARTUP, 0),
			              Obj_crd(_tid.exc_pt_sel + PT_SEL_STARTUP, 0))) {
				PERR("could not create startup portal");
				throw Cpu_session::Thread_creation_failed();
			}

			/* remap debugging page fault portal for core threads */
			if (map_local((Utcb *)Thread_base::myself()->utcb(),
			              Obj_crd(PT_SEL_PAGE_FAULT, 0),
			              Obj_crd(_tid.exc_pt_sel + PT_SEL_PAGE_FAULT, 0))) {
				PERR("could not create page fault portal");
				throw Cpu_session::Thread_creation_failed();
			}

			/* default: we don't accept any mappings or translations */
			Utcb * utcb_obj = reinterpret_cast<Utcb *>(Thread_base::utcb());
			utcb_obj->crd_rcv = Obj_crd();
			utcb_obj->crd_xlt = Obj_crd();

			/* create SC */
			unsigned sc_sel = cap_map()->insert();
			res = create_sc(sc_sel, pd_sel, _tid.ec_sel, Qpd(Qpd::DEFAULT_QUANTUM, Qpd::DEFAULT_PRIORITY + 1));
			if (res != NOVA_OK) {
				PERR("%p - create_sc returned returned %d", this, res);
				throw Cpu_session::Thread_creation_failed();
			}
		}
};


/**
 * Irq_proxy interface implementation
 */
class Genode::Irq_proxy_component : public Irq_proxy<Irq_thread>
{
	private:

		Genode::addr_t _irq_sel; /* IRQ cap selector */
		Genode::addr_t _dev_mem; /* used when MSI or HPET is used */

	protected:

		bool _associate()
		{
			/* alloc slector where IRQ will be mapped */
			_irq_sel = cap_map()->insert();

			/* since we run in APIC mode translate IRQ 0 (PIT) to 2 */
			if (!_irq_number)
				_irq_number = 2;

			/* map IRQ number to selector */
			int ret = map_local((Nova::Utcb *)Thread_base::myself()->utcb(),
			                    Nova::Obj_crd(platform_specific()->gsi_base_sel() + _irq_number, 0),
			                    Nova::Obj_crd(_irq_sel, 0),
			                    true);
			if (ret) {
				PERR("Could not map IRQ %ld", _irq_number);
				return false;
			}

			/* assign IRQ to CPU */
			addr_t msi_addr = 0;
			addr_t msi_data = 0;
			uint8_t res = Nova::assign_gsi(_irq_sel, _dev_mem, boot_cpu(),
			                               msi_addr, msi_data);
			if (res != Nova::NOVA_OK)
				PERR("Error: assign_pci failed -irq:dev:msi_addr:msi_data "
				     "%lx:%lx:%lx:%lx", _irq_number, _dev_mem, msi_addr,
				     msi_data);

			return res == Nova::NOVA_OK;
		}

		void _wait_for_irq()
		{
			if (Nova::sm_ctrl(_irq_sel, Nova::SEMAPHORE_DOWN))
				nova_die();
		}

		void _ack_irq() { }

	public:

		Irq_proxy_component(long irq_number)
		:
			Irq_proxy(irq_number), _dev_mem(0)
		{
			_start();
		}
};




/***************************
 ** IRQ session component **
 ***************************/


void Irq_session_component::ack_irq()
{
	if (!_proxy) {
		PERR("Expected to find IRQ proxy for IRQ %02x", _irq_number);
		return;
	}

	_proxy->ack_irq();
}


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1) {
		PERR("invalid IRQ number requested");
		throw Root::Unavailable();
	}

	/* check if IRQ thread was started before */
	typedef Irq_proxy<Irq_thread> Proxy;
	_proxy = Proxy::get_irq_proxy<Irq_proxy_component>(irq_number, irq_alloc);
	if (!_proxy) {
		PERR("unavailable IRQ %lx requested", irq_number);
		throw Root::Unavailable();
	}

	_irq_number = irq_number;
}


Irq_session_component::~Irq_session_component()
{
	if (!_proxy)
		return;

	if (_irq_sigh.valid())
		_proxy->remove_sharer(&_irq_sigh);
}


void Irq_session_component::sigh(Genode::Signal_context_capability sigh)
{
	if (!_proxy) {
		PERR("signal handler got not registered - irq thread unavailable");
		return;
	}

	Genode::Signal_context_capability old = _irq_sigh;

	if (old.valid() && !sigh.valid())
		_proxy->remove_sharer(&_irq_sigh);

	_irq_sigh = sigh;

	if (!old.valid() && sigh.valid())
		_proxy->add_sharer(&_irq_sigh);
}
