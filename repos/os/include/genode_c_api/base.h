/*
 * \brief  C interface to Genode's base types
 * \author Norman Feske
 * \date   2021-07-06
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GENODE_C_API__BASE_H_
#define _INCLUDE__GENODE_C_API__BASE_H_

#ifdef __cplusplus
extern "C" {
#endif


/**********************************************
 ** Forward-declared types used in the C API **
 **********************************************/

struct genode_env;
struct genode_allocator;
struct genode_signal_handler;
struct genode_attached_dataspace;

#ifdef __cplusplus
} /* extern "C" */
#endif


/**********************************************************************
 ** Mapping between C types and their corresponding Genode C++ types **
 **********************************************************************/

#ifdef __cplusplus

#include <base/env.h>
#include <base/allocator.h>
#include <base/attached_dataspace.h>

struct genode_env                : Genode::Env { };
struct genode_allocator          : Genode::Allocator { };
struct genode_signal_handler     : Genode::Signal_dispatcher_base { };
struct genode_attached_dataspace : Genode::Attached_dataspace { };

static inline auto genode_env_ptr(Genode::Env &env)
{
	return static_cast<genode_env *>(&env);
}

static inline auto genode_allocator_ptr(Genode::Allocator &alloc)
{
	return static_cast<genode_allocator *>(&alloc);
}

static inline auto genode_signal_handler_ptr(Genode::Signal_dispatcher_base &sigh)
{
	return static_cast<genode_signal_handler *>(&sigh);
}

static inline Genode::Signal_context_capability cap(genode_signal_handler *sigh_ptr)
{
	Genode::Signal_dispatcher_base *dispatcher_ptr = sigh_ptr;
	return *static_cast<Genode::Signal_handler<Genode::Meta::Empty> *>(dispatcher_ptr);
}

static inline auto genode_attached_dataspace_ptr(Genode::Attached_dataspace &ds)
{
	return static_cast<genode_attached_dataspace *>(&ds);
}

#endif

#endif /* _INCLUDE__GENODE_C_API__BASE_H_ */
