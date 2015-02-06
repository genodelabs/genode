/*
 * \brief   Kernel backend for execution contexts in userland
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
#include <kernel/thread.h>

using namespace Kernel;


Thread::Thread(unsigned const priority, unsigned const quota,
               char const * const label)
:
	Thread_base(this), Cpu_job(priority, quota), _state(AWAITS_START), _pd(0),
	_utcb_phys(0), _signal_receiver(0), _label(label)
{ PDBG("not implemented"); }


void Thread::exception(unsigned const cpu) {
	PDBG("not implemented"); }
