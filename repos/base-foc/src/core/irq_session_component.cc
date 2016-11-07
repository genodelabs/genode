/*
 * \brief  Fiasco.OC-specific core implementation of IRQ sessions
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <util/arg_string.h>
#include <util/bit_array.h>

/* core includes */
#include <irq_root.h>
#include <irq_args.h>
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

namespace Genode {
	class Interrupt_handler;
}


using namespace Genode;


/**
 * Dispatches interrupts from kernel
 */
class Genode::Interrupt_handler : public Thread_deprecated<2048*sizeof(long)>
{
	private:

		Interrupt_handler() : Thread_deprecated("irq_handler") { start(); }

	public:

		void entry();

		static Fiasco::l4_cap_idx_t handler_cap()
		{
			static Interrupt_handler handler;
			return handler._thread_cap.data()->kcap();
		}

};


enum { MAX_MSIS = 256 };
static class Msi_allocator : public Genode::Bit_array<MAX_MSIS>
{
	public:

		Msi_allocator() {
			using namespace Fiasco;

			l4_icu_info_t info { .features = 0 };
			l4_msgtag_t res = l4_icu_info(Fiasco::L4_BASE_ICU_CAP, &info);

			if (l4_error(res) || !(info.features & L4_ICU_FLAG_MSI))
				set(0, MAX_MSIS);
			else
				if (info.nr_msis < MAX_MSIS)
					set(info.nr_msis, MAX_MSIS - info.nr_msis);
		}
} msi_alloc;

bool Genode::Irq_object::associate(unsigned irq, bool msi,
                                   Irq_session::Trigger trigger,
                                   Irq_session::Polarity polarity)
{
	if (msi)
		/*
		 * Local APIC address, See Intel x86 Spec - Section MSI 10.11.
		 *
		 * XXX local Apic ID encoding missing - address is constructed
		 *     assuming that local APIC id of boot CPU is 0 XXX
		 */
		_msi_addr = 0xfee00000UL;

	_irq      = irq;
	_trigger  = trigger;
	_polarity = polarity;

	using namespace Fiasco;
	if (l4_error(l4_factory_create_irq(L4_BASE_FACTORY_CAP, _capability()))) {
		error("l4_factory_create_irq failed!");
		return false;
	}

	unsigned long gsi = _irq;
	if (_msi_addr)
		gsi |= L4_ICU_FLAG_MSI;

	if (l4_error(l4_icu_bind(L4_BASE_ICU_CAP, gsi, _capability()))) {
		error("Binding IRQ ", _irq, " to the ICU failed");
		return false;
	}

	if (!_msi_addr)
		/* set interrupt mode */
		Platform::setup_irq_mode(gsi, _trigger, _polarity);

	if (l4_error(l4_irq_attach(_capability(), reinterpret_cast<l4_umword_t>(this),
	                           Interrupt_handler::handler_cap()))) {
		error("cannot attach to IRQ ", _irq);
		return false;
	}

	if (_msi_addr && l4_error(l4_icu_msi_info(L4_BASE_ICU_CAP, gsi,
	                                          &_msi_data))) {
		error("cannot get MSI info");
		return false;
	}

	return true;
}


void Genode::Irq_object::ack_irq()
{
	using namespace Fiasco;

	int err;
	l4_msgtag_t tag = l4_irq_unmask(_capability());
	if ((err = l4_ipc_error(tag, l4_utcb())))
		error("IRQ unmask: ", err);
}


Genode::Irq_object::Irq_object()
:
	 _cap(cap_map()->insert(platform_specific()->cap_id_alloc()->alloc())),
	 _trigger(Irq_session::TRIGGER_UNCHANGED),
	 _polarity(Irq_session::POLARITY_UNCHANGED),
	 _irq(~0U), _msi_addr(0), _msi_data(0)
{ }


Genode::Irq_object::~Irq_object()
{
	if (_irq == ~0U)
		return;

	using namespace Fiasco;

	unsigned long gsi = _irq;
	if (_msi_addr)
		gsi |= L4_ICU_FLAG_MSI;

	if (l4_error(l4_irq_detach(_capability())))
		error("cannot detach IRQ");

	if (l4_error(l4_icu_unbind(L4_BASE_ICU_CAP, gsi, _capability())))
		error("cannot unbind IRQ");

	cap_map()->remove(_cap);
}


/***************************
 ** IRQ session component **
 ***************************/


Irq_session_component::Irq_session_component(Range_allocator *irq_alloc,
                                             const char      *args)
:
	_irq_number(~0U), _irq_alloc(irq_alloc)
{
	Irq_args const irq_args(args);

	long msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);
	if (msi) {
		if (msi_alloc.get(irq_args.irq_number(), 1)) {
			error("unavailable MSI ", irq_args.irq_number(), " requested");
			throw Root::Unavailable();
		}
		msi_alloc.set(irq_args.irq_number(), 1);
	} else {
		if (!irq_alloc || irq_alloc->alloc_addr(1, irq_args.irq_number()).error()) {
			error("unavailable IRQ ", irq_args.irq_number(), " requested");
			throw Root::Unavailable();
		}
	}

	_irq_number = irq_args.irq_number();

	_irq_object.associate(_irq_number, msi, irq_args.trigger(),
	                      irq_args.polarity());
}


Irq_session_component::~Irq_session_component()
{
	if (_irq_number == ~0U)
		return;

	if (_irq_object.msi_address()) {
		msi_alloc.clear(_irq_number, 1);
	} else {
		Genode::addr_t free_irq = _irq_number;
		_irq_alloc->free((void *)free_irq);
	}
}


void Irq_session_component::ack_irq() { _irq_object.ack_irq(); }


void Irq_session_component::sigh(Genode::Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Genode::Irq_session::Info Irq_session_component::info()
{
	if (!_irq_object.msi_address() || !_irq_object.msi_value())
		return { .type = Genode::Irq_session::Info::Type::INVALID };

	return {
		.type    = Genode::Irq_session::Info::Type::MSI,
		.address = _irq_object.msi_address(),
		.value   = _irq_object.msi_value()
	};
}


/**************************************
 ** Interrupt handler implementation **
 **************************************/

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
			error("IRQ receive: ", err);
		else {
			Irq_object * irq_object = reinterpret_cast<Irq_object *>(label);
			irq_object->notify();
		}
	}
}
