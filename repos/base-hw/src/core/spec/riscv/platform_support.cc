/*
 * \brief   Platform implementations specific for RISC-V
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
#include <platform.h>
#include <board.h>
#include <cpu.h>

#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


void Platform::_init_io_port_alloc() { }

void Platform::_init_additional() { }

void Platform::setup_irq_mode(unsigned, unsigned, unsigned) { }

long Platform::irq(long const user_irq) { return 0; }

bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number) {
	return false; }
