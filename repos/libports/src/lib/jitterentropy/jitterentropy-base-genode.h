/*
 * \brief  Genode base for jitterentropy
 * \author Josef Soentgen
 * \author Christian Helmuth
 * \date   2014-08-18
 *
 * Required to be included outside of extern "C" {...} at top of
 * jitterentropy.h.
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _JITTERENTROPY_BASE_GENODE_H_
#define _JITTERENTROPY_BASE_GENODE_H_

#ifdef __cplusplus

#include <base/allocator.h>

/* Genode specific function to set the backend allocator */
void jitterentropy_init(Genode::Allocator &alloc);

#endif

#endif /* _JITTERENTROPY_BASE_GENODE_H_ */
