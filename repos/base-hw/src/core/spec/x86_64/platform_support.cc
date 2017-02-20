/*
 * \brief   Platform implementations specific for x86_64
 * \author  Reto Buerki
 * \date    2015-05-04
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <kernel/kernel.h>

using namespace Genode;


void Platform::_init_additional() { };


void Platform::setup_irq_mode(unsigned irq_number, unsigned trigger,
                              unsigned polarity) {
	Kernel::pic()->ioapic.setup_irq_mode(irq_number, trigger, polarity); }


bool Platform::get_msi_params(const addr_t mmconf, addr_t &address,
                              addr_t &data, unsigned &irq_number) {
	return false; }
