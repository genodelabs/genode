/*
 * \brief  Interface of 2D-copy library
 * \author Norman Feske
 * \date   2007-10-10
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLIT__BLIT_H_
#define _INCLUDE__BLIT__BLIT_H_

/**
 * Blit memory from source buffer to destination buffer
 *
 * \param src    address of source buffer
 * \param src_w  line length of source buffer in bytes
 * \param dst    address of destination buffer
 * \param dst_w  line length of destination buffer in bytes
 * \param w      number of bytes per line to copy
 * \param h      number of lines to copy
 *
 * This function works at a granularity of 16bit.
 * If the source and destination overlap, the result
 * of the copy operation is not defined.
 */
extern "C" void blit(void const *src, unsigned src_w,
                     void *dst, unsigned dst_w, int w, int h);

#endif /* _INCLUDE__BLIT__BLIT_H_ */
