/*
 * \brief  CPU-specific memcpy
 * \author Sebastian Sumpf
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__SPEC__X86__CPU__STRING_H_
#define _INCLUDE__SPEC__X86__CPU__STRING_H_

namespace Genode {

	/**
	 * Copy memory block
	 *
	 * \param dst   destination memory block
	 * \param src   source memory block
	 * \param size  number of bytes to copy
	 *
	 * \return      number of bytes not copied
	 */
	inline size_t memcpy_cpu(void *, const void *, size_t size) { return size; }
}

#endif /* _INCLUDE__SPEC__X86__CPU__STRING_H_ */
