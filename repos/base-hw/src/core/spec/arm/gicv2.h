/*
 * \brief  Board support for GICv2
 * \author Stefan Kalkowski
 * \date   2025-10-22
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__ARM__GICV2_H_
#define _CORE__SPEC__ARM__GICV2_H_

/* base-hw internal includes */
#include <hw/spec/arm/gicv2.h>

namespace Board {

	using Hw::Global_interrupt_controller;

	/**
	 * Unfortunately, we have to derive here from the actual
	 * implementation in namespace Hw, because Local_interrupt_controller
	 * needs to be forward declared in kernel/irq.h
	 */
	struct Local_interrupt_controller : Hw::Local_interrupt_controller {
		using Hw::Local_interrupt_controller::Local_interrupt_controller; };
}

#endif /* _CORE__SPEC__ARM__GICV2_H_ */

