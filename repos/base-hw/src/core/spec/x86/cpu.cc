/*
 * \brief   Kernel backend for protection domains
 * \author  Stefan Kalkowski
 * \date    2015-03-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>
#include <kernel/pd.h>

void Genode::Cpu::init_virt_kernel(Kernel::Pd * pd) {
	Cr3::write(Cr3::init((addr_t)pd->translation_table())); }
