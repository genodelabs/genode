/*
 * \brief  Utilities for implementing VMMs on Genode/NOVA
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VMM__TYPES_H_
#define _INCLUDE__VMM__TYPES_H_

namespace Vmm {

	enum {
		PAGE_SIZE_LOG2 = 12UL,
		PAGE_SIZE      = PAGE_SIZE_LOG2 << 12
	};
}

#endif /* _INCLUDE__VMM__TYPES_H_ */
