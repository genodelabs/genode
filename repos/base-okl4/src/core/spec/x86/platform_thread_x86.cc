/*
 * \brief   x86-specific OKL4 thread facility
 * \author  Christian Prochaska
 * \date    2011-04-15
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform_thread.h>

/* base-internal includes */
#include <base/internal/okl4.h>

using namespace Core;
using namespace Okl4;


Thread_state Platform_thread::state()
{
	Thread_state s { };

	L4_Copy_regs_to_mrs(_l4_thread_id);

	enum {
		MR_EIP    = 0,
		MR_EFLAGS = 1,
		MR_EDI    = 2,
		MR_ESI    = 3,
		MR_EBP    = 4,
		MR_ESP    = 5,
		MR_EBX    = 6,
		MR_EDX    = 7,
		MR_ECX    = 8,
		MR_EAX    = 9,
	};

	L4_StoreMR(MR_EIP,    &s.cpu.ip);
	L4_StoreMR(MR_EFLAGS, &s.cpu.eflags);
	L4_StoreMR(MR_EDI,    &s.cpu.edi);
	L4_StoreMR(MR_ESI,    &s.cpu.esi);
	L4_StoreMR(MR_EBP,    &s.cpu.ebp);
	L4_StoreMR(MR_ESP,    &s.cpu.sp);
	L4_StoreMR(MR_EBX,    &s.cpu.ebx);
	L4_StoreMR(MR_EDX,    &s.cpu.edx);
	L4_StoreMR(MR_ECX,    &s.cpu.ecx);
	L4_StoreMR(MR_EAX,    &s.cpu.eax);

	return s;
}


void Platform_thread::state(Thread_state) { }
