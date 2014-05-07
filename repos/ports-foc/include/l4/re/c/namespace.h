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

#ifndef _L4__RE__C__NAMESPACE_H_
#define _L4__RE__C__NAMESPACE_H_

#include <l4/re/env.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef l4_cap_idx_t l4re_namespace_t;

L4_CV long l4re_ns_query_srv(l4re_namespace_t srv, char const *name,
                             l4_cap_idx_t const cap);

#ifdef __cplusplus
}
#endif

#endif /* _L4__RE__C__NAMESPACE_H_ */
