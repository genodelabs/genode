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

/* Genode includes */
#include <spec/arm/cpu/vcpu_state_trustzone.h>

/* core includes */
#include <types.h>

namespace Kernel { class Cpu; }

namespace Board {

	using Core::Vcpu_state;
	using Vcpu_data = Core::Vcpu_state;

	enum { VCPU_MAX = 1 };

	struct Vm_page_table {};
	struct Vm_page_table_array {};

	class Global_interrupt_controller { public: void init() {} };
	struct Pic : Hw::Pic { struct Virtual_context {}; Pic(Global_interrupt_controller &) { } };
	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}

#endif /* _CORE__SPEC__ARM_TRUSTZONE_BOARD_H_ */
