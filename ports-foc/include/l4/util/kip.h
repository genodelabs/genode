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

#ifndef _L4__UTIL__KIP_H_
#define _L4__UTIL__KIP_H_

#include <l4/sys/kip.h>
#include <l4/sys/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

L4_CV int l4util_kip_kernel_has_feature(l4_kernel_info_t *, const char *str);
L4_CV unsigned long l4util_kip_kernel_abi_version(l4_kernel_info_t *);

#ifdef __cplusplus
}
#endif

#endif /* _L4__UTIL__KIP_H_ */
