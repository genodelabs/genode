/*
 * \brief  Guarded MSR access on NOVA
 * \author Alexander Boettcher
 * \date   2023-10-03
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SPEC_X86_64__NOVA_MSR_H_
#define _CORE__INCLUDE__SPEC_X86_64__NOVA_MSR_H_

#include <pd_session_component.h>

static Genode::Pd_session::Managing_system_state msr_access(Genode::Pd_session::Managing_system_state const &state,
                                                            Nova::Utcb &utcb,
                                                            Genode::addr_t const msr_cap)
{
	Genode::Pd_session::Managing_system_state result { };

	utcb.set_msg_word(state.ip); /* count */
	utcb.msg()[0] = state.r8;
	utcb.msg()[1] = state.r9;
	utcb.msg()[2] = state.r10;
	utcb.msg()[3] = state.r11;
	utcb.msg()[4] = state.r12;
	utcb.msg()[5] = state.r13;
	utcb.msg()[6] = state.r14;
	utcb.msg()[7] = state.r15;

	auto const res = Nova::ec_ctrl(Nova::Ec_op::EC_MSR_ACCESS, msr_cap);

	result.trapno = (res == Nova::NOVA_OK) ? 1 : 0;

	if (res != Nova::NOVA_OK)
		return result;

	result.ip  = utcb.msg_words(); /* bitmap about valid returned words */
	result.r8  = utcb.msg()[0];
	result.r9  = utcb.msg()[1];
	result.r10 = utcb.msg()[2];
	result.r11 = utcb.msg()[3];
	result.r12 = utcb.msg()[4];
	result.r13 = utcb.msg()[5];
	result.r14 = utcb.msg()[6];
	result.r15 = utcb.msg()[7];

	return result;
}

#endif /* _CORE__INCLUDE__SPEC_X86_64__NOVA_MSR_H_ */
