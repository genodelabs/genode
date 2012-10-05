/*
 * \brief  Fiasco.OC-specific core implementation of IRQ sessions
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \date   2007-09-13
 *
 * FIXME ram quota missing
 */

/*
 * Copyright (C) 2007-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
#include <irq_proxy_component.h>
#include <irq_session_component.h>
#include <platform.h>
#include <util.h>

/* Fiasco includes */
namespace Fiasco {
#include <l4/sys/icu.h>
#include <l4/sys/irq.h>
#include <l4/sys/factory.h>
#include <l4/sys/types.h>
}

using namespace Genode;

namespace Genode {
	class Interrupt_handler;
	class Irq_proxy_component;
}

/**
 * Dispatches interrupts from kernel
 */
class Genode::Interrupt_handler : public Thread<4096>
{
	private:

		Interrupt_handler() { start(); }

	public:

		void entry();

		static Native_thread handler_cap()
		{
			static Interrupt_handler handler;
			return handler._thread_cap.dst();
		}
};


/**
 * Irq_proxy interface implementation
 */
class Genode::Irq_proxy_component : public Irq_proxy_base
{
	private:

		Cap_index *_cap;
		Semaphore  _sem;
		long       _trigger;    /* interrupt trigger */
		long       _polarity; /* interrupt polarity */

		Native_thread _capability() const { return _cap->kcap(); }

	protected:

		bool _associate()
		{
			using namespace Fiasco;
			if (l4_error(l4_factory_create_irq(L4_BASE_FACTORY_CAP, _capability()))) {
				PERR("l4_factory_create_irq failed!");
				return false;
			}

			if (l4_error(l4_icu_bind(L4_BASE_ICU_CAP, _irq_number, _capability()))) {
				PERR("Binding IRQ%ld to the ICU failed", _irq_number);
				return false;
			}

			/* set interrupt mode */
			Platform::setup_irq_mode(_irq_number, _trigger, _polarity);

			if (l4_error(l4_irq_attach(_capability(), _irq_number,
				                         Interrupt_handler::handler_cap()))) {
				PERR("Error attaching to IRQ %ld", _irq_number);
				return false;
			}

			return true;
		}

		void _wait_for_irq()
		{
			using namespace Fiasco;

			int err;
			l4_msgtag_t tag = l4_irq_unmask(_capability());
			if ((err = l4_ipc_error(tag, l4_utcb())))
				PERR("IRQ unmask: %d\n", err);

			_sem.down();
		}

		void _ack_irq() { }

	public:

		Irq_proxy_component(long irq_number)
		:
		 Irq_proxy_base(irq_number),
		 _cap(cap_map()->insert(platform_specific()->cap_id_alloc()->alloc())),
		 _sem(), _trigger(-1), _polarity(-1) { }

		Semaphore *semaphore() { return &_sem; }

		void start(long trigger, long polarity)
		{
			_trigger    = trigger;
			_polarity = polarity;
			_start();
		}

		bool match_mode(long trigger, long polarity)
		{
			if (trigger == Irq_session::TRIGGER_UNCHANGED &&
			    polarity == Irq_session::POLARITY_UNCHANGED)
			 return true;

			if (_trigger < 0 && _polarity < 0)
				return true;

			return _trigger == trigger && _polarity == polarity;
		}

		long trigger()  const { return _trigger; }
		long polarity() const { return _polarity; }
};


/********************************
 ** IRQ session implementation **
 ********************************/


Irq_session_component::Irq_session_component(Cap_session     *cap_session,
                                             Range_allocator *irq_alloc,
                                             const char      *args)
:
	_ep(cap_session, STACK_SIZE, "irqctrl"),
	_proxy(0)
{
	using namespace Fiasco;

	long irq_number = Arg_string::find_arg(args, "irq_number").long_value(-1);
	if (irq_number == -1) {
		PERR("Unavailable IRQ %lx requested", irq_number);
		throw Root::Invalid_args();
	}
	
	long irq_trigger = Arg_string::find_arg(args, "irq_trigger").long_value(-1);
	irq_trigger = irq_trigger == -1 ? 0 : irq_trigger;

	long irq_polarity = Arg_string::find_arg(args, "irq_polarity").long_value(-1);
	irq_polarity = irq_polarity == -1 ? 0 : irq_polarity;

	/*
	 * temorary hack for fiasco.oc using the local-apic,
	 * where old pic-line 0 maps to 2
	 */
	if (irq_number == 0)
		irq_number = 2;

	if (!(_proxy = Irq_proxy_component::get_irq_proxy<Irq_proxy_component>(irq_number,
	                                                                       irq_alloc))) {
		PERR("No proxy for IRQ %lu found", irq_number);
		throw Root::Unavailable();
	}

	/* sanity check  */
	if (!_proxy->match_mode(irq_trigger, irq_polarity)) {
		PERR("Interrupt mode mismatch: IRQ %ld current mode: t: %ld p: %ld"
		     "request mode: trg: %ld p: %ld",
		     irq_number, _proxy->trigger(), _proxy->polarity(),
		     irq_trigger, irq_polarity);
		throw Root::Unavailable();
	}

	/* set interrupt mode and start proxy */
	_proxy->start(irq_trigger, irq_polarity);

	if (!_proxy->add_sharer())
		throw Root::Unavailable();

	/* initialize capability */
	_irq_cap = _ep.manage(this);
}


void Irq_session_component::wait_for_irq()
{
	_proxy->wait_for_irq();
}


Irq_session_component::~Irq_session_component()
{
	PERR("Implement me, immediately!");
}


/***************************************
 ** Interrupt handler implemtentation **
 ***************************************/

void Interrupt_handler::entry()
{
	using namespace Fiasco;

	int         err;
	l4_msgtag_t tag;
	l4_umword_t label;

	while (true) {
		tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
		if ((err = l4_ipc_error(tag, l4_utcb())))
			PERR("IRQ receive: %d\n", err);
		else {
			Irq_proxy_component *proxy;
			proxy = Irq_proxy_component::get_irq_proxy<Irq_proxy_component>(label);

			if (proxy)
				proxy->semaphore()->up();
		}
	}
}
