/*
 * \brief   Platform implementations specific for base-hw and Raspberry Pi
 * \author  Norman Feske
 * \date    2013-04-05
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <cpu.h>

using namespace Genode;

Cpu::User_context::User_context() { cpsr = Psr::init_user(); }
