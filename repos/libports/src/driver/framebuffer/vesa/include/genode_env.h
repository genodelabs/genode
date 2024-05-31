/*
 * \brief  Utilities for accessing the Genode environment
 * \author Norman Feske
 * \date   2017-01-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GENODE_ENV_H_
#define _GENODE_ENV_H_

#include <base/component.h>
#include <base/log.h>

static Genode::Env *_env_ptr;
static inline Genode::Env &genode_env()
{
	if (_env_ptr)
		return *_env_ptr;

	Genode::error("genode env accessed prior initialization");
	throw 1;
}


static Genode::Allocator *_alloc_ptr;
static inline Genode::Allocator &alloc()
{
	if (_alloc_ptr)
		return *_alloc_ptr;

	Genode::error("allocator accessed prior initialization");
	throw 1;
}


static void local_init_genode_env(Genode::Env &env, Genode::Allocator &alloc)
{
	_env_ptr   = &env;
	_alloc_ptr = &alloc;
}

#endif /* _GENODE_ENV_H_ */
