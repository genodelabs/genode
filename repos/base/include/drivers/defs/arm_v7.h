/*
 * \brief  Definitions for Armv7
 * \author Stefan Kalkowski
 * \date   2019-04-10
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__DEFS__ARM_V7_H_
#define _INCLUDE__DRIVERS__DEFS__ARM_V7_H_

namespace Arm_v7 {

	enum Interrupts {

		/******************************
		 ** Virtualization extension **
		 ******************************/

		VT_MAINTAINANCE_IRQ = 25,
		VT_TIMER_IRQ        = 27,
	};
};

#endif /* _INCLUDE__DRIVERS__DEFS__ARM_V7_H_ */
