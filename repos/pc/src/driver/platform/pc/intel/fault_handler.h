/*
 * \brief  Intel IOMMU fault handler
 * \author Johannes Schlatow
 * \date   2025-04-11
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__DRIVERS__PLATFORM__INTEL__FAULT_HANDLER_H_
#define _SRC__DRIVERS__PLATFORM__INTEL__FAULT_HANDLER_H_

#include <util/interface.h>

namespace Intel {

	class Fault_handler : Interface
	{
		public:

			virtual bool iq_error() = 0;
			virtual void handle_faults() = 0;
	};
}

#endif /* _SRC__DRIVERS__PLATFORM__INTEL__FAULT_HANDLER_H_ */
