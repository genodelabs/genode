/*
 * \brief   ARM cpu context initialization when TrustZone is used
 * \author  Stefan Kalkowski
 * \date    2017-04-12
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <spec/arm/cpu_support.h>

void Genode::Arm_cpu::User_context::init(bool privileged)
{
	using Psr = Genode::Arm_cpu::Psr;

	Psr::access_t v = 0;
	Psr::M::set(v, privileged ? Psr::M::SYS : Psr::M::USR);
	Psr::I::set(v, 1);
	Psr::A::set(v, 1);
	regs->cpsr = v;
}
