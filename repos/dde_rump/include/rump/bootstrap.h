/*
 * \brief  Rump initialization
 * \author Norman Feske
 * \date   2016-11-02
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _INCLUDE__RUMP__BOOTSTRAP_H_
#define _INCLUDE__RUMP__BOOTSTRAP_H_

#include <base/env.h>
#include <base/allocator.h>

void rump_bootstrap_init(Genode::Env &env, Genode::Allocator &heap);

#endif /* _INCLUDE__RUMP__BOOTSTRAP_H_ */
