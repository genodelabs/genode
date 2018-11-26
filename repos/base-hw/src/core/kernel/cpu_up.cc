/*
 * \brief  Kernel cpu object implementations for uniprocessors
 * \author Stefan Kalkowski
 * \date   2018-11-08
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <kernel/cpu.h>

void Kernel::Cpu::Ipi::occurred() { }


void Kernel::Cpu::trigger_ip_interrupt() { }


Kernel::Cpu::Ipi::Ipi(Kernel::Cpu & cpu) : Irq(~0U, cpu), cpu(cpu) { }
