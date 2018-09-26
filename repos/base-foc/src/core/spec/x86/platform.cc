/*
 * \brief  Platform support specific to x86
 * \author Christian Helmuth
 * \author Stefan Kalkowski
 * \date   2006-04-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <irq_session/irq_session.h>
#include <util/xml_generator.h>

#include "platform.h"
#include "util.h"

/* Fiasco.OC includes */
namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/icu.h>
#include <l4/sys/kip.h>
}

void Genode::Platform::_setup_io_port_alloc()
{
	using namespace Fiasco;

	l4_fpage_t fpage = l4_iofpage(0, L4_WHOLE_IOADDRESS_SPACE);
	l4_utcb_mr()->mr[0] = fpage.raw;
	l4_utcb_br()->bdr  &= ~L4_BDR_OFFSET_MASK;
	l4_utcb_br()->br[0] = L4_ITEM_MAP;
	l4_utcb_br()->br[1] = fpage.raw;
	l4_msgtag_t tag = l4_ipc_call(L4_BASE_PAGER_CAP, l4_utcb(),
	                              l4_msgtag(L4_PROTO_IO_PAGE_FAULT, 1, 0, 0),
	                              L4_IPC_NEVER);

	if (l4_ipc_error(tag, l4_utcb()))
		panic("Received no I/O ports from sigma0");

	/* setup allocator */
	_io_port_alloc.add_range(0, 0x10000);
}


void Genode::Platform::setup_irq_mode(unsigned irq_number, unsigned trigger,
                                      unsigned polarity)
{
	using namespace Fiasco;

	l4_umword_t mode;

	/* set edge-high as default for legacy interrupts */
	if (irq_number < 16
	 && trigger  == Irq_session::TRIGGER_UNCHANGED
	 && polarity == Irq_session::TRIGGER_UNCHANGED) {

		mode = L4_IRQ_F_POS_EDGE;

	} else {

		/*
		 * Translate ACPI interrupt mode (trigger/polarity) to Fiasco APIC
		 * values. Default is level low
		 */
		if  (trigger == Irq_session::TRIGGER_LEVEL || trigger == Irq_session::TRIGGER_UNCHANGED) {
			if (polarity == Irq_session::POLARITY_LOW || polarity == Irq_session::POLARITY_UNCHANGED)
				mode = L4_IRQ_F_LEVEL_LOW;
			else
				mode = L4_IRQ_F_LEVEL_HIGH;
		}
		else {
			if (polarity == Irq_session::POLARITY_LOW || polarity == Irq_session::POLARITY_UNCHANGED)
				mode = L4_IRQ_F_NEG_EDGE;
			else
				mode = L4_IRQ_F_POS_EDGE;
		}
	}

	/*
	 * Set mode
	 */
	if (l4_error(l4_icu_set_mode(L4_BASE_ICU_CAP, irq_number, mode)))
		error("setting mode for IRQ ", irq_number, " failed");
}

static bool cpu_name(char const * name) {
	using Genode::uint32_t;

	uint32_t cpuid = 0, edx = 0, ebx = 0, ecx = 0;
	asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx), "=b"(ebx), "=c"(ecx));

	return ebx == *reinterpret_cast<uint32_t const *>(name) &&
	       edx == *reinterpret_cast<uint32_t const *>(name + 4) &&
	       ecx == *reinterpret_cast<uint32_t const *>(name + 8);
}

void Genode::Platform::_setup_platform_info(Xml_generator &xml,
                                            Fiasco::l4_kernel_info_t &kip)
{
	xml.node("features", [&] () {
		/* XXX better detection required, best told us by kernel !? */
		xml.attribute("svm", cpu_name("AuthenticAMD"));
		xml.attribute("vmx", cpu_name("GenuineIntel"));
	});
	xml.node("tsc", [&] () {
		xml.attribute("freq_khz" , kip.frequency_cpu);
	});
}
