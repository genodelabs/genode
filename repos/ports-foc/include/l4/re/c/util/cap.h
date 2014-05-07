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

#ifndef _L4__RE__C__UTIL__CAP_H_
#define _L4__RE__C__UTIL__CAP_H_

#include <l4/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV l4_msgtag_t l4re_util_cap_release(l4_cap_idx_t cap);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__C__UTIL__CAP_H_ */
