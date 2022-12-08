/*
 * \brief  CPU-specific memcpy
 * \author Sebastian Sumpf
 * \author Johanned Schlatow
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CPU__STRING_H_
#define _INCLUDE__CPU__STRING_H_

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
	inline size_t memcpy_cpu(void * dst, const void * src, size_t size)
	{
		typedef unsigned long word_t;

		enum {
			LEN  = sizeof(word_t),
			MASK = LEN-1
		};

		unsigned char *d = (unsigned char *)dst, *s = (unsigned char *)src;

		/* check byte alignment */
		size_t d_align = (size_t)d & MASK;
		size_t s_align = (size_t)s & MASK;

		/* only same alignments work */
		if (d_align != s_align)
			return size;

		/* copy to word alignment */
		for (; (size > 0) && (s_align > 0) && (s_align < LEN);
		     s_align++, *d++ = *s++, size--);

		/* copy words */
		for (; size >= LEN; size -= LEN,
		                       d += LEN,
		                       s += LEN)
			*(word_t*)d = *(word_t*)s;

		return size;
	}
}

#endif /* _INCLUDE__CPU__STRING_H_ */
