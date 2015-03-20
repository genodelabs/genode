/*
 * \brief  Environment of a component
 * \author Norman Feske
 * \date   2006-07-01
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__ENV_H_
#define _INCLUDE__BASE__ENV_H_

#include <parent/capability.h>
#include <parent/parent.h>
#include <rm_session/rm_session.h>
#include <ram_session/ram_session.h>
#include <cpu_session/cpu_session.h>
#include <cpu_session/capability.h>
#include <pd_session/pd_session.h>
#include <base/allocator.h>
#include <base/snprintf.h>
#include <base/lock.h>

namespace Genode {

	struct Env;

	/**
	 * Return the interface to the component's environment
	 */
	extern Env *env();
}


/**
 * Component runtime environment
 *
 * The environment of a Genode component is defined by its parent. The 'Env'
 * class allows the component to interact with its environment. It is
 * initialized at the startup of the component.
 */
struct Genode::Env
{
	virtual ~Env() { }

	/**
	 * Communication channel to our parent
	 */
	virtual Parent *parent() = 0;

	/**
	 * RAM session of the component
	 *
	 * The RAM Session represents a budget of memory (quota) that is
	 * available to the component. This budget can be used to allocate
	 * RAM dataspaces.
	 */
	virtual Ram_session *ram_session() = 0;
	virtual Ram_session_capability ram_session_cap() = 0;

	/**
	 * CPU session of the component
	 *
	 * This session is used to create the threads of the component.
	 */
	virtual Cpu_session *cpu_session() = 0;
	virtual Cpu_session_capability cpu_session_cap() = 0;

	/**
	 * Region-manager session of the component as created by the parent
	 */
	virtual Rm_session *rm_session() = 0;

	/**
	 * PD session of the component as created by the parent
	 */
	virtual Pd_session *pd_session() = 0;

	/**
	 * Heap backed by the RAM session of the environment
	 */
	virtual Allocator *heap() = 0;
};



#endif /* _INCLUDE__BASE__ENV_H_ */
