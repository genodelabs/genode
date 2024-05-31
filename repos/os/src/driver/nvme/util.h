/*
 * \brief  Utilitize used by the NVMe driver
 * \author Josef Soentgen
 * \date   2018-03-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NVME_UTIL_H_
#define _NVME_UTIL_H_

/* Genode includes */
#include <base/fixed_stdint.h>

namespace Util {

	using namespace Genode;

	/**
	 * Extract string from memory
	 *
	 * This function is used to extract the information strings from the
	 * identify structure.
	 */
	char const *extract_string(char const *base, size_t offset, size_t len)
	{
		static char tmp[64] = { };
		if (len > sizeof(tmp)) { return nullptr; }

		copy_cstring(tmp, base + offset, len);

		len--; /* skip NUL */
		while (len > 0 && tmp[--len] == ' ') { tmp[len] = 0; }
		return tmp;
	}
}

#endif /* _NVME_UTIL_H_ */
