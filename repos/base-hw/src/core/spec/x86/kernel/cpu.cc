/*
 * \brief   Class for kernel data that is needed to manage a specific CPU
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/cpu.h>

using namespace Kernel;


Cpu_idle::Cpu_idle(Cpu * const cpu) : Cpu_job(Cpu_priority::min, 0) {
	PDBG("not implemented"); }


void Cpu_idle::exception(unsigned const cpu) {
	PDBG("not implemented"); }
