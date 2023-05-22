/*
 * \brief  Size protected wrapper for the manual placement of objects
 * \author Martin Stein
 * \date   2023-03-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__CONSTRUCT_IN_BUF_H_
#define _TRESOR__CONSTRUCT_IN_BUF_H_

/* base includes */
#include <util/construct_at.h>

namespace Tresor {

	using namespace Genode;

	template <typename    T,
	          typename... ARGS>
	static inline void construct_in_buf(void       *buf_ptr,
	                                    size_t      buf_size,
	                                    ARGS &&...  args)
	{
		if (sizeof(T) > buf_size) {
			class Buffer_too_small { };
			throw Buffer_too_small { };
		}
		construct_at<T>(buf_ptr, args...);
	}
}

#endif /* _TRESOR__CONSTRUCT_IN_BUF_H_ */
