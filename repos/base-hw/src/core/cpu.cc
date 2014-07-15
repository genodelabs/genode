/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2014-07-14
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>

using namespace Genode;

unsigned Cpu::primary_id() { return 0; }

unsigned Cpu::executing_id() { return primary_id(); }
