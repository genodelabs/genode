/*
 * \brief   ARM cpu context initialization
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

Genode::Arm_cpu::User_context::User_context()
{
	using Psr = Arm_cpu::Psr;

	Psr::access_t v = 0;
	Psr::M::set(v, Psr::M::USR);
	Psr::F::set(v, 1);
	Psr::A::set(v, 1);
	cpsr = v;
}
