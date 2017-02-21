/*
 * \brief  CPU-specific initialization code for Arndale
 * \author Stefan Kalkowski
 * \date   2016-01-07
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu.h>
#include <translation_table.h>



void Genode::Cpu::init(Genode::Translation_table & table)
{
	prepare_nonsecure_world();
	prepare_hypervisor(table);
	switch_to_supervisor_mode();
}
