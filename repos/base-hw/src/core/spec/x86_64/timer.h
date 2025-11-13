/*
 * \brief  Timer driver for core
 * \author Adrian-Ken Rueegsegger
 * \author Reto Buerki
 * \date   2015-02-06
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__CORE__SPEC__ARM__PIT_H_
#define _SRC__CORE__SPEC__ARM__PIT_H_

/* Genode includes */
#include <base/stdint.h>
#include <trace/timestamp.h>

/* hw includes */
#include <hw/spec/x86_64/apic.h>
#include <hw/spec/x86_64/cpu.h>

namespace Board { class Timer; }


/**
 * LAPIC-based timer driver for core
 */
struct Board::Timer: public Hw::Apic
{
	Genode::uint8_t          divider          = 0;
	Genode::uint32_t         ticks_per_ms     = 0;
	Genode::Trace::Timestamp tsc_ticks_per_ms = 0;

	Timer(Hw::X86_64_cpu::Id);

	void init();
};

#endif /* _SRC__CORE__SPEC__ARM__PIT_H_ */
