/*
 * \brief  Declarations for boards without any Vcpu support
 * \author Stefan Kalkowski
 * \date   2025-04-10
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__NO_VCPU_BOARD_H_
#define _CORE__NO_VCPU_BOARD_H_

namespace Genode {
	struct Vcpu_state {};
}

namespace Kernel { class Cpu; }

namespace Board {
	struct Vcpu_context { Vcpu_context(Kernel::Cpu &) {} };
}

#endif /* _CORE__NO_VCPU_BOARD_H_ */

