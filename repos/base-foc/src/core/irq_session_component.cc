/*
 * \brief  Fiasco.OC-specific core implementation of IRQ sessions
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \author Sebastian Sumpf
 * \date   2007-09-13
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <util/bit_array.h>

/* core includes */
#include <irq_root.h>
#include <irq_args.h>
#include <irq_session_component.h>
#include <platform.h>
#include <util.h>

/* Fiasco.OC includes */
#include <foc/syscall.h>

namespace Core { class Interrupt_handler; }

using namespace Core;


/**
 * Dispatches interrupts from kernel
 */
class Core::Interrupt_handler : public Thread
{
	private:

		Interrupt_handler()
		:
			Thread(Weight::DEFAULT_WEIGHT, "irq_handler",
			       2048 * sizeof(long) /* stack */, Type::NORMAL)
		{ start(); }

	public:

		void entry() override;

		static Foc::l4_cap_idx_t handler_cap()
		{
			static Interrupt_handler handler;
			return handler._thread_cap.data()->kcap();
		}

};


enum { MAX_MSIS = 256 };


struct Msi_allocator : Bit_array<MAX_MSIS>
{
	Msi_allocator()
	{
		using namespace Foc;

		l4_icu_info_t info { };

		l4_msgtag_t const res = l4_icu_info(Foc::L4_BASE_ICU_CAP, &info);

		if (l4_error(res) || !(info.features & L4_ICU_FLAG_MSI))
			set(0, MAX_MSIS);
		else
			if (info.nr_msis < MAX_MSIS)
				set(info.nr_msis, MAX_MSIS - info.nr_msis);
	}

};


static Msi_allocator & msi_alloc()
{
	static Msi_allocator instance;
	return instance;
}


bool Irq_object::associate(unsigned irq, bool msi,
                           Irq_session::Trigger trigger,
                           Irq_session::Polarity polarity)
{
	using namespace Foc;

	_irq      = irq;
	_trigger  = trigger;
	_polarity = polarity;

	if (msi)
		irq |= L4_ICU_FLAG_MSI;
	else
		/* set interrupt mode */
		Platform::setup_irq_mode(irq, _trigger, _polarity);

	if (l4_error(l4_factory_create_irq(L4_BASE_FACTORY_CAP, _capability()))) {
		error("l4_factory_create_irq failed!");
		return false;
	}
	if (l4_error(l4_icu_bind(L4_BASE_ICU_CAP, irq, _capability()))) {
		error("Binding IRQ ", _irq, " to the ICU failed");
		return false;
	}

	if (l4_error(l4_rcv_ep_bind_thread(_capability(), Interrupt_handler::handler_cap(),
	                                   reinterpret_cast<l4_umword_t>(this)))) {
		error("cannot attach to IRQ ", _irq);
		return false;
	}

	if (msi) {
		/**
		 * src_id represents bit 64-84 of the Interrupt Remap Table Entry Format
		 * for Remapped Interrupts, reference section 9.10 of the
		 * Intel ® Virtualization Technology for Directed I/O
		 * Architecture Specification
		 */
		unsigned src_id = 0x0;
		Foc::l4_icu_msi_info_t info = l4_icu_msi_info_t();
		if (l4_error(l4_icu_msi_info(L4_BASE_ICU_CAP, irq,
		                             src_id, &info))) {
			error("cannot get MSI info");
			return false;
		}
		_msi_addr = info.msi_addr;
		_msi_data = info.msi_data;
	}

	return true;
}


void Irq_object::ack_irq()
{
	using namespace Foc;

	l4_umword_t err;
	l4_msgtag_t tag = l4_irq_unmask(_capability());
	if ((err = l4_ipc_error(tag, l4_utcb())))
		error("IRQ unmask: ", err);
}


Irq_object::Irq_object()
:
	 _cap(cap_map().insert((Cap_index::id_t)platform_specific().cap_id_alloc().alloc())),
	 _trigger(Irq_session::TRIGGER_UNCHANGED),
	 _polarity(Irq_session::POLARITY_UNCHANGED),
	 _irq(~0U), _msi_addr(0), _msi_data(0)
{ }


Irq_object::~Irq_object()
{
	if (_irq == ~0U)
		return;

	using namespace Foc;

	unsigned long irq = _irq;

	if (_msi_addr)
		irq |= L4_ICU_FLAG_MSI;

	if (l4_error(l4_irq_detach(_capability())))
		error("cannot detach IRQ");

	if (l4_error(l4_icu_unbind(L4_BASE_ICU_CAP, (unsigned)irq, _capability())))
		error("cannot unbind IRQ");

	cap_map().remove(_cap);
}


/***************************
 ** IRQ session component **
 ***************************/

Irq_session_component::Irq_session_component(Range_allocator &irq_alloc,
                                             const char      *args)
:
	_irq_number((unsigned)Arg_string::find_arg(args, "irq_number").long_value(-1)),
	_irq_alloc(irq_alloc), _irq_object()
{
	long const msi = Arg_string::find_arg(args, "device_config_phys").long_value(0);

	if (msi) {
		if (msi_alloc().get(_irq_number, 1)) {
			error("unavailable MSI ", _irq_number, " requested");
			throw Service_denied();
		}
		msi_alloc().set(_irq_number, 1);
	} else {
		if (irq_alloc.alloc_addr(1, _irq_number).failed()) {
			error("unavailable IRQ ", _irq_number, " requested");
			throw Service_denied();
		}
	}

	Irq_args const irq_args(args);

	if (_irq_object.associate(_irq_number, msi, irq_args.trigger(),
	                          irq_args.polarity()))
		return;

	/* cleanup */
	if (msi)
		msi_alloc().clear(_irq_number, 1);
	else {
		addr_t const free_irq = _irq_number;
		_irq_alloc.free((void *)free_irq);
	}
	throw Service_denied();
}


Irq_session_component::~Irq_session_component()
{
	if (_irq_number == ~0U)
		return;

	if (_irq_object.msi_address()) {
		msi_alloc().clear(_irq_number, 1);
	} else {
		addr_t const free_irq = _irq_number;
		_irq_alloc.free((void *)free_irq);
	}
}


void Irq_session_component::ack_irq()
{
	_irq_object.ack_irq();
}


void Irq_session_component::sigh(Signal_context_capability cap)
{
	_irq_object.sigh(cap);
}


Irq_session::Info Irq_session_component::info()
{
	if (!_irq_object.msi_address())
		return { .type = Info::Type::INVALID, .address = 0, .value = 0 };

	return {
		.type    = Irq_session::Info::Type::MSI,
		.address = (addr_t)_irq_object.msi_address(),
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
	using namespace Foc;

	l4_umword_t err;
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
