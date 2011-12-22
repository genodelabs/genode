/*
 * \brief   x86-specific OKL4 thread facility
 * \author  Christian Prochaska
 * \date    2011-04-15
 */

/*
 * Copyright (C) 2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <platform_thread.h>

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/thread.h>
} }

using namespace Genode;
using namespace Okl4;


int Platform_thread::state(Thread_state *state_dst)
{
	state_dst->tid = _l4_thread_id;

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

	L4_StoreMR(MR_EIP,    &state_dst->ip);
	L4_StoreMR(MR_EFLAGS, &state_dst->eflags);
	L4_StoreMR(MR_EDI,    &state_dst->edi);
	L4_StoreMR(MR_ESI,    &state_dst->esi);
	L4_StoreMR(MR_EBP,    &state_dst->ebp);
	L4_StoreMR(MR_ESP,    &state_dst->sp);
	L4_StoreMR(MR_EBX,    &state_dst->ebx);
	L4_StoreMR(MR_EDX,    &state_dst->edx);
	L4_StoreMR(MR_ECX,    &state_dst->ecx);
	L4_StoreMR(MR_EAX,    &state_dst->eax);

	return 0;
}
