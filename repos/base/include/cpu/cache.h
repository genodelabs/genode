/*
 * \brief  Cache operations
 * \author Christian Prochaska
 * \date   2014-05-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU__CACHE_H_
#define _INCLUDE__CPU__CACHE_H_

#include <base/stdint.h>

namespace Genode {

	/*
	 * Make D-Cache and I-Cache coherent
	 */
	void cache_coherent(Genode::addr_t addr, Genode::size_t size);
}

#endif /* _INCLUDE__CPU__CACHE_H_ */
