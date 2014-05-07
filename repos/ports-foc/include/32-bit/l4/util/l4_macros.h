/*
 * \brief  L4Re functions needed by L4Linux
 * \author Stefan Kalkowski
 * \date   2011-03-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _L4__UTIL__L4_MACROS_H_
#define _L4__UTIL__L4_MACROS_H_

#define l4util_idfmt "%lx"
#define l4_addr_fmt  "%08lx"
#define l4util_idstr(tid) (tid >> L4_CAP_SHIFT)

#endif /* _L4__UTIL__L4_MACROS_H_ */
