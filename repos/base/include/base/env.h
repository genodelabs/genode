/*
 * \brief  Component environment
 * \author Norman Feske
 * \date   2015-12-17
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__ENV_H_
#define _INCLUDE__BASE__ENV_H_

#include <parent/parent.h>
#include <base/entrypoint.h>
#include <cpu_session/capability.h>
#include <pd_session/capability.h>

namespace Genode { struct Env; }


struct Genode::Env : Interface
{
	virtual Parent &parent() = 0;

	/**
	 * CPU session of the component
	 *
	 * This session is used to create the threads of the component.
	 */
	virtual Cpu_session &cpu() = 0;

	/**
	 * Region map of the component's address space
	 */
	virtual Region_map &rm() = 0;

	/**
	 * PD session of the component as created by the parent
	 */
	virtual Pd_session &pd() = 0;

	/**
	 * Memory allocator
	 */
	Ram_allocator &ram() { return pd(); }

	/**
	 * Entrypoint for handling RPC requests and signals
	 */
	virtual Entrypoint &ep() = 0;

	/**
	 * Return the CPU-session capability of the component
	 */
	virtual Cpu_session_capability cpu_session_cap() = 0;

	/**
	 * Return the PD-session capability of the component
	 */
	virtual Pd_session_capability pd_session_cap()  = 0;

	/**
	 * ID space of sessions obtained from the parent
	 */
	virtual Id_space<Parent::Client> &id_space() = 0;

	virtual Session_capability session(Parent::Service_name const &,
	                                   Parent::Client::Id,
	                                   Parent::Session_args const &,
	                                   Affinity             const &) = 0;

	/**
	 * Create session to a service
	 *
	 * \param SESSION_TYPE     session interface type
	 * \param id               session ID of new session
	 * \param args             session constructor arguments
	 * \param affinity         preferred CPU affinity for the session
	 *
	 * \throw Service_denied
	 * \throw Insufficient_cap_quota
	 * \throw Insufficient_ram_quota
	 * \throw Out_of_caps
	 * \throw Out_of_ram
	 *
	 * See the documentation of 'Parent::session'.
	 *
	 * This method blocks until the session is available or an error
	 * occurred.
	 */
	template <typename SESSION_TYPE>
	Capability<SESSION_TYPE> session(Parent::Client::Id          id,
	                                 Parent::Session_args const &args,
	                                 Affinity             const &affinity)
	{
		Session_capability cap = session(SESSION_TYPE::service_name(),
		                                 id, args, affinity);
		return static_cap_cast<SESSION_TYPE>(cap);
	}

	/**
	 * Upgrade session quota
	 *
	 * \param id    ID of recipient session
	 * \param args  description of the amount of quota to transfer
	 *
	 * \throw Out_of_ram
	 * \throw Out_of_caps
	 *
	 * See the documentation of 'Parent::upgrade'.
	 *
	 * The 'args' argument has the same principle format as the 'args'
	 * argument of the 'session' operation.
	 */
	virtual void upgrade(Parent::Client::Id id,
	                     Parent::Upgrade_args const &args) = 0;

	/**
	 * Close session and block until the session is gone
	 */
	virtual void close(Parent::Client::Id) = 0;

	/**
	 * Excecute pending static constructors
	 *
	 * On component startup, the dynamic linker does not call possible static
	 * constructors in the binary and shared libraries the binary depends on. If
	 * the component requires static construction it needs to call this function
	 * at construction time explicitly. For example, the libc implementation
	 * executes this function before constructing libc components.
	 */
	virtual void exec_static_constructors() = 0;

	/**
	 * Reload parent capability and reinitialize environment resources
	 *
	 * This method is solely used for implementing fork in Noux. After forking
	 * a process, the new child process is executed within a copy of the
	 * address space of the forking process. Thereby, the new process inherits
	 * the original 'env' object of the forking process, which is meaningless
	 * in the context of the new process. By calling this function, the new
	 * process is able to reinitialize its 'env' with meaningful capabilities
	 * obtained via its updated parent capability.
	 *
	 * \noapi
	 */
	virtual void reinit(Native_capability::Raw) = 0;

	/**
	 * Reinitialize main-thread object
	 *
	 * \param stack_area_rm  new region map of the stack area
	 *
	 * This function is solely used for implementing fork as provided by the
	 * Noux environment.
	 *
	 * \noapi
	 */
	virtual void reinit_main_thread(Capability<Region_map> &stack_area_rm) = 0;
};

#endif /* _INCLUDE__BASE__ENV_H_ */
