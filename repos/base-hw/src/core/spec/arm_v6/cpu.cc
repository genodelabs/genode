/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2014-08-06
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>

using namespace Genode;

void Arm::Context::translation_table(addr_t const table) {
	ttbr0 = Cpu::Ttbr0::init(table); }
