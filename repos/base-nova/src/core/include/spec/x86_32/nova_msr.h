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

#ifndef _CORE__INCLUDE__SPEC_X86_32__NOVA_MSR_H_
#define _CORE__INCLUDE__SPEC_X86_32__NOVA_MSR_H_

#include <pd_session_component.h>

static Genode::Pd_session::Managing_system_state msr_access(Genode::Pd_session::Managing_system_state const &,
                                                            Nova::Utcb &,
                                                            Genode::addr_t const)
{
	return { }; /* not supported for now on x86_32 */
}

#endif /* _CORE__INCLUDE__SPEC_X86_32__NOVA_MSR_H_ */
