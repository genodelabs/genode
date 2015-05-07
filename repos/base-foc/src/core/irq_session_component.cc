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
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <util/arg_string.h>

/* core includes */
#include <irq_root.h>
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
	typedef Irq_proxy<Thread<1024 * sizeof(addr_t)> > Irq_proxy_base;
}

/**
 * Dispatches interrupts from kernel
 */
class Genode::Interrupt_handler : public Thread<4096>
{
	private:

		Interrupt_handler() : Thread("irq_handler") { start(); }

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

		Cap_index             *_cap;
		Semaphore              _sem;
		Irq_session::Trigger   _trigger;  /* interrupt trigger */
		Irq_session::Polarity  _polarity; /* interrupt polarity */

		bool                   _running;
		Genode::addr_t         _msi_addr;
		Genode::addr_t         _msi_data;

		Native_thread _capability() const { return _cap->kcap(); }

	protected:

		bool _associate()
		{
			using namespace Fiasco;
			if (l4_error(l4_factory_create_irq(L4_BASE_FACTORY_CAP,
			                                   _capability()))) {
				PERR("l4_factory_create_irq failed!");
				return false;
			}

			unsigned long gsi = _irq_number;
			if (_msi_addr)
				gsi |= L4_ICU_FLAG_MSI;

			if (l4_error(l4_icu_bind(L4_BASE_ICU_CAP, gsi, _capability()))) {
				PERR("Binding IRQ%ld to the ICU failed", _irq_number);
				return false;
			}

			if (!_msi_addr)
				/* set interrupt mode */
				Platform::setup_irq_mode(gsi, _trigger, _polarity);

			if (l4_error(l4_irq_attach(_capability(), _irq_number,
				                         Interrupt_handler::handler_cap()))) {
				PERR("Error attaching to IRQ %ld", _irq_number);
				return false;
			}

			if (_msi_addr && l4_error(l4_icu_msi_info(L4_BASE_ICU_CAP, gsi,
			                                          &_msi_data))) {
				PERR("Error getting MSI info");
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
		 _sem(), _trigger(Irq_session::TRIGGER_UNCHANGED),
		 _polarity(Irq_session::POLARITY_UNCHANGED),
		 _running(false), _msi_addr(0), _msi_data(0)
		{ }

		Semaphore *semaphore() { return &_sem; }

		Irq_session::Trigger  trigger()  const { return _trigger; }
		Irq_session::Polarity polarity() const { return _polarity; }

		void setup_irq_mode(Irq_session::Trigger t, Irq_session::Polarity p)
		{
			_trigger  = t;
			_polarity = p;

			/* set interrupt mode */
			Platform::setup_irq_mode(_irq_number, _trigger, _polarity);
		}

		Genode::addr_t msi_address() const { return _msi_addr; }
		Genode::addr_t msi_value()   const { return _msi_data; }

		void enable(bool msi)
		{
			if (_running)
				return;

			if (msi)
				/*
				 * Local APIC address, See Intel x86 Spec - Section MSI 10.11.
				 *
				 * XXX local Apic ID encoding missing - address is constructed
				 *     assuming that local APIC id of boot CPU is 0 XXX
				 */
				_msi_addr = 0xfee00000UL;

			_running = true;
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
	if (irq_number < 0) {
		PERR("invalid IRQ number requested %ld", irq_number);
		throw Root::Unavailable();
	}

	long irq_t = Arg_string::find_arg(args, "irq_trigger").long_value(-1);
	long irq_p = Arg_string::find_arg(args, "irq_polarity").long_value(-1);

	Irq_session::Trigger irq_trigger;
	Irq_session::Polarity irq_polarity;

	switch(irq_t) {
		case -1:
		case Irq_session::TRIGGER_UNCHANGED:
			irq_trigger = Irq_session::TRIGGER_UNCHANGED;
			break;
		case Irq_session::TRIGGER_EDGE:
			irq_trigger = Irq_session::TRIGGER_EDGE;
			break;
		case Irq_session::TRIGGER_LEVEL:
			irq_trigger = Irq_session::TRIGGER_LEVEL;
			break;
		default:
			throw Root::Unavailable();
	}

	switch(irq_p) {
		case -1:
		case POLARITY_UNCHANGED:
			irq_polarity = POLARITY_UNCHANGED;
			break;
		case POLARITY_HIGH:
			irq_polarity = POLARITY_HIGH;
			break;
		case POLARITY_LOW:
			irq_polarity = POLARITY_LOW;
			break;
		default:
			throw Root::Unavailable();
	}

	long msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (msi) {
		using namespace Fiasco;

		l4_icu_info_t info { .features = 0 };
		l4_msgtag_t res = l4_icu_info(Fiasco::L4_BASE_ICU_CAP, &info);
		if (l4_error(res) || !(info.features & L4_ICU_FLAG_MSI))
			throw Root::Unavailable();

		/**
		 * irq_alloc range [0, max)
		 *
		 *  legacy irq [0, max_legacy)
		 *  msi        [max_legacy, info.nr_msis)
		 *  unused     [info.nr_msis, max)
		 *
		 *  max        - is currently set to 256 in base-foc platform.cc
		 *  max_legacy - is info.nr_irqs, defined by hardware/kernel
		 */
		irq_number = info.nr_msis - 1 - irq_number;
	}

	/* check if IRQ thread was started before */
	_proxy = Irq_proxy_component::get_irq_proxy<Irq_proxy_component>(irq_number, irq_alloc);
	if (!_proxy) {
		PERR("unavailable IRQ %lx requested", irq_number);
		throw Root::Unavailable();
	}

	bool setup = false;
	bool fail  = false;
	/* sanity check  */
	if (irq_trigger != TRIGGER_UNCHANGED && _proxy->trigger() != irq_trigger) {
		if (_proxy->trigger() == TRIGGER_UNCHANGED)
			setup = true;
		else
			fail = true;
	}

	if (irq_polarity != POLARITY_UNCHANGED && _proxy->polarity() != irq_polarity) {
		if (_proxy->polarity() == POLARITY_UNCHANGED)
			setup = true;
		else
			fail = true;
	}

	if (fail) {
		PERR("Interrupt mode mismatch: IRQ %ld current mode: t: %d p: %d "
		     "request mode: trg: %d p: %d",
		     irq_number, _proxy->trigger(), _proxy->polarity(),
		     irq_trigger, irq_polarity);
		throw Root::Unavailable();
	}

	if (setup)
		/* set interrupt mode */
		_proxy->setup_irq_mode(irq_trigger, irq_polarity);

	_irq_number = irq_number;

	_proxy->enable(msi);
}


Irq_session_component::~Irq_session_component()
{
	if (!_proxy) return;

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


Genode::Irq_session::Info Irq_session_component::info()
{
	if (!_proxy || !_proxy->msi_address() || !_proxy->msi_value())
		return { .type = Genode::Irq_session::Info::Type::INVALID };

	return {
		.type    = Genode::Irq_session::Info::Type::MSI,
		.address = _proxy->msi_address(),
		.value   = _proxy->msi_value()
	};
}


/***************************************
 ** Interrupt handler implemtentation **
 ***************************************/

/* Build with frame pointer to make GDB backtraces work. See issue #1061. */
__attribute__((optimize("-fno-omit-frame-pointer")))
__attribute__((noinline))
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
