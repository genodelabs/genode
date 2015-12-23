/*
 * \brief  Hook functions for bootstrapping the component
 * \author Norman Feske
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__COMPONENT_H_
#define _INCLUDE__BASE__COMPONENT_H_

#include <base/stdint.h>
#include <parent/parent.h>
#include <base/entrypoint.h>
#include <ram_session/capability.h>
#include <cpu_session/capability.h>
#include <rm_session/rm_session.h>
#include <pd_session/pd_session.h>


namespace Genode { struct Environment; }


/**
 * Interface to be provided by the component implementation
 */
namespace Component
{
	Genode::size_t stack_size();
	char const *name();
	void construct(Genode::Environment &);
}


struct Genode::Environment
{
	virtual Parent &parent() = 0;

	/**
	 * RAM session of the component
	 *
	 * The RAM Session represents a budget of memory (quota) that is available
	 * to the component. This budget can be used to allocate RAM dataspaces.
	 */
	virtual Ram_session &ram() = 0;

	/**
	 * CPU session of the component
	 *
	 * This session is used to create the threads of the component.
	 */
	virtual Cpu_session &cpu() = 0;

	/**
	 * Region-manager session of the component as created by the parent
	 */
	virtual Rm_session &rm() = 0;

	/**
	 * PD session of the component as created by the parent
	 */
	virtual Pd_session &pd() = 0;

	/**
	 * Entrypoint for handling RPC requests and signals
	 */
	virtual Entrypoint &ep() = 0;

	virtual Ram_session_capability ram_session_cap() = 0;
	virtual Cpu_session_capability cpu_session_cap() = 0;
};

#endif /* _INCLUDE__BASE__COMPONENT_H_ */
