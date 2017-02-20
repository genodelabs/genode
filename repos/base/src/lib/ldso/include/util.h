/**
 * \brief  Helper functions
 * \author Sebastian Sumpf
 * \date   2015-03-12
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL_H_
#define _INCLUDE__UTIL_H_

namespace Linker {

	/**
	 * Page handling
	 */
	template <typename T>
	static inline T trunc_page(T addr) {
		return addr & _align_mask((T)12); }

	template <typename T>
	static inline T round_page(T addr) {
		return align_addr(addr, (T)12); }

	/**
	 * Extract file name from path
	 */
	inline char const *file(char const *path)
	{
		/* strip directories */
			char  const *f, *r = path;
			for (f = r; *f; f++)
				if (*f == '/')
					r = f + 1;
			return r;
	}
}

#endif /* _INCLUDE__UTIL_H_ */
