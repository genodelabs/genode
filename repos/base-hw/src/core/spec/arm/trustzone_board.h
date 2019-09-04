/*
 * \brief  Board driver
 * \author Stefan Kalkowski
 * \date   2019-11-11
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_
#define _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_

#include <spec/arm/cpu/vm_state_trustzone.h>

namespace Kernel { class Cpu; }

namespace Board {
	using Genode::Vm_state;

	enum { VCPU_MAX = 1 };

	struct Vm_page_table {};
	struct Vm_page_table_array {};

	struct Pic : Hw::Pic { struct Virtual_context {}; };
	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}

#endif /* _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_ */
