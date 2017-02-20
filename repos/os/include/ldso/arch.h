/*
 * \brief  Architecture-specific functions
 * \author Sebastian Sumpf
 * \date   2009-10-26
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LDSO_ARCH_H_
#define _LDSO_ARCH_H_

#include <dataspace/capability.h>

namespace Genode {
	void set_parent_cap_arch(void *ptr);
	int binary_name(Dataspace_capability ds_cap, char *buf, size_t buf_size);
}

#endif //_LDSO_ARCH_H_
