/*
 * \brief   Kernel back-end and core front-end for user interrupts
 * \author  Martin Stein
 * \date    2013-10-28
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/irq.h>
#include <pic.h>
#include <processor_driver.h>

using namespace Kernel;

namespace Kernel { Pic * pic(); }

void Irq::_disable() const { pic()->mask(_id()); }

void Irq::_enable() const { pic()->unmask(_id(), Genode::Cpu::id()); }
