/*
 * \brief  Power State Coordination Interface for ARMv8 virtualization
 * \author Stefan Kalkowski
 * \date   2019-10-21
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__PSCI_H_
#define _SRC__SERVER__VMM__PSCI_H_

namespace Vmm {

	namespace Psci {
		enum Function_id {
			PSCI_VERSION      = 0x84000000,
			MIGRATE_INFO_TYPE = 0x84000006,
			PSCI_FEATURES     = 0x8400000a,
			CPU_ON            = 0xc4000003,
		};

		enum {
			VERSION       = 1 << 16, /* 1.0 */
			SUCCESS       = 0,
			NOT_SUPPORTED = -1,
		};
	};
};

#endif /* _SRC__SERVER__VMM__PSCI_H_ */
