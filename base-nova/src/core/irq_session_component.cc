/*
 * \brief  Implementation of IRQ session component
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
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

		static void _thread_start()
		{
			Thread_base::myself()->entry();
			sleep_forever();
		}

	public:

		Irq_thread(char const *name) : Thread_base(name, 1024 * sizeof(addr_t)) { }

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
			addr_t *sp   = reinterpret_cast<addr_t *>(_context->stack - sizeof(addr_t));
			*sp = reinterpret_cast<addr_t>(_thread_start);
	
			/* create global EC */
			enum { CPU_NO = 0, GLOBAL = true };
			uint8_t res = create_ec(_tid.ec_sel, pd_sel, CPU_NO,
			                        utcb, (mword_t)sp, _tid.exc_pt_sel, GLOBAL);
			if (res != NOVA_OK) {
				PERR("%p - create_ec returned %d", this, res);
				throw Cpu_session::Thread_creation_failed();
			}

			/* map startup portal from main thread */
			map_local((Utcb *)Thread_base::myself()->utcb(),
			          Obj_crd(PT_SEL_STARTUP, 0),
			          Obj_crd(_tid.exc_pt_sel + PT_SEL_STARTUP, 0));

			/* create SC */
			unsigned sc_sel = cap_selector_allocator()->alloc();
			res = create_sc(sc_sel, pd_sel, _tid.ec_sel, Qpd());
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

		long _irq_sel; /* IRQ cap selector */

	protected:

		bool _associate()
		{
			/* alloc slector where IRQ will be mapped */
			_irq_sel = cap_selector_allocator()->alloc();
		
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
			enum { CPU = 0 };
			Nova::assign_gsi(_irq_sel, 0, CPU);

			return true;
		}

		void _wait_for_irq()
		{
			if (Nova::sm_ctrl(_irq_sel, Nova::SEMAPHORE_DOWN))
				nova_die();
		}

		void _ack_irq() { }

	public:

		Irq_proxy_component(long irq_number) : Irq_proxy(irq_number)
		{
			_start();
		}
};


typedef Irq_proxy<Irq_thread> Proxy;


void Irq_session_component::wait_for_irq()
{
	_proxy->wait_for_irq();
	/* interrupt ocurred and proxy woke us up */
}


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_ep(cap_session, STACK_SIZE, "irq")
{
	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);

	/* check if IRQ thread was started before */
	_proxy = Proxy::get_irq_proxy<Irq_proxy_component>(irq_number, irq_alloc);
	if (irq_number == -1 || !_proxy) {
		PERR("Unavailable IRQ %lx requested", irq_number);
		throw Root::Unavailable();
	}

	_proxy->add_sharer();

	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


Irq_session_component::~Irq_session_component() { }
