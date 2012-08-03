/*
 * \brief  Cpu specifi memcpy
 * \author Sebastian Sumpf
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__X86__CPU__STRING_H_
#define _INCLUDE__X86__CPU__STRING_H_

namespace Genode
{
	/**
	 * Copy memory block
	 *
	 * \param dst   destination memory block
	 * \param src   source memory block
	 * \param size  number of bytes to copy
	 *
	 * \return      Number of bytes not copied
	 */
	inline size_t memcpy_cpu(void *dst, const void *src, size_t size) {
		return size; }
}

#endif /* _INCLUDE__X86__CPU__STRING_H_ */
