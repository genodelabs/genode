/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Sebastian Sumpf
 * \date    2015-06-02
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <kernel/cpu.h>
#include <hw/memory_map.h>

void Kernel::Cpu::init(Kernel::Pic &) {
	Stvec::write(Hw::Mm::supervisor_exception_vector().base); }
