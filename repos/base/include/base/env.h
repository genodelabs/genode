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
#include <ram_session/capability.h>
#include <cpu_session/capability.h>
#include <pd_session/capability.h>
#include <rm_session/rm_session.h>

/* maintain compatibility to deprecated API */
#include <deprecated/env.h>


namespace Genode { struct Env; }


struct Genode::Env
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
	 * Region map of the component's address space
	 */
	virtual Region_map &rm() = 0;

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

	/*
	 * XXX temporary
	 *
	 * The PD session capability is solely used for upgrading the PD session,
	 * e.g., when the dynamic linker attaches dataspaces to the linker area.
	 * Once we add 'Env::upgrade', we can remove this accessor.
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
	 * \throw Service_denied   parent denies session request
	 * \throw Quota_exceeded   our own quota does not suffice for
	 *                         the creation of the new session
	 * \throw Unavailable
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
	 * \throw Quota_exceeded  quota could not be transferred
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
};

#endif /* _INCLUDE__BASE__ENV_H_ */
