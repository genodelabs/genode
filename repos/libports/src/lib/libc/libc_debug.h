/*
 * \brief  Debugging hooks
 * \author Norman Feske
 * \date   2009-05-26
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_DEBUG_H_
#define _LIBC_DEBUG_H_

#define LIBC_DEBUG 0

#if (LIBC_DEBUG == 1)

/*
 * This function is implemented in base-linux (env lib)
 */
extern "C" int raw_write_str(const char *s);

#else

/*
 * Discard external references to 'raw_write_str'
 */
static inline int raw_write_str(const char *s) { }

#endif /* LIBC_DEBUG == 1 */

#endif /* _LIBC_DEBUG_H_ */
