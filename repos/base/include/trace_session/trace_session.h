/*
 * \brief  TRACE session interface
 * \author Norman Feske
 * \date   2013-08-11
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TRACE_SESSION__TRACE_SESSION_H_
#define _INCLUDE__TRACE_SESSION__TRACE_SESSION_H_

#include <base/exception.h>
#include <base/trace/types.h>
#include <dataspace/capability.h>
#include <session/session.h>

namespace Genode { namespace Trace { struct Session; } }


struct Genode::Trace::Session : Genode::Session
{
	static const char *service_name() { return "TRACE"; }

	/**
	 * Allocate policy-module backing store
	 *
	 * \throw Out_of_metadata
	 */
	virtual Policy_id alloc_policy(size_t size) = 0;

	/**
	 * Request policy-module backing store
	 *
	 * \throw Nonexistent_policy
	 */
	virtual Dataspace_capability policy(Policy_id) = 0;

	/**
	 * Remove a policy module from the TRACE service
	 */
	virtual void unload_policy(Policy_id) = 0;

	/**
	 * Start tracing of a subject
	 *
	 * \throw Out_of_metadata
	 * \throw Already_traced
	 * \throw Source_is_dead
	 * \throw Nonexistent_policy
	 * \throw Traced_by_other_session
	 */
	virtual void trace(Subject_id, Policy_id, size_t buffer_size) = 0;

	/**
	 * Install a matching rule for automatically tracing new threads
	 */
	virtual void rule(Session_label const &, Thread_name const &,
	                  Policy_id, size_t buffer_size) = 0;

	/**
	 * Pause generation of tracing data
	 *
	 * \throw Nonexistent_subject
	 */
	virtual void pause(Subject_id) = 0;

	/**
	 * Resume generation of tracing data
	 *
	 * \throw Nonexistent_subject
	 * \throw Source_is_dead
	 */
	virtual void resume(Subject_id) = 0;

	/**
	 * Obtain details about tracing subject
	 *
	 * \throw Nonexistent_subject
	 */
	virtual Subject_info subject_info(Subject_id) = 0;

	/**
	 * Obtain trace buffer of given subject
	 *
	 * \throw Nonexistent_subject
	 */
	virtual Dataspace_capability buffer(Subject_id) = 0;

	/**
	 * Release subject and free buffers
	 *
	 * If the source still exists, the buffers are freed but the subject
	 * stays intact.
	 *
	 * \throw Nonexistent_subject
	 */
	virtual void free(Subject_id) = 0;

	virtual ~Session() { }


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC_THROW(Rpc_alloc_policy, Policy_id, alloc_policy,
	                 GENODE_TYPE_LIST(Out_of_metadata),
	                 size_t);
	GENODE_RPC_THROW(Rpc_policy, Dataspace_capability, policy,
	                 GENODE_TYPE_LIST(Nonexistent_policy),
	                 Policy_id);
	GENODE_RPC_THROW(Rpc_unload_policy, void, unload_policy,
	                 GENODE_TYPE_LIST(Nonexistent_policy), Policy_id);
	GENODE_RPC_THROW(Rpc_trace, void, trace,
	                 GENODE_TYPE_LIST(Out_of_metadata, Already_traced,
	                                  Source_is_dead, Nonexistent_policy,
	                                  Traced_by_other_session),
	                 Subject_id, Policy_id, size_t);
	GENODE_RPC_THROW(Rpc_rule, void, rule,
	                 GENODE_TYPE_LIST(Out_of_metadata),
	                 Session_label const &, Thread_name const &,
	                 Policy_id, size_t);
	GENODE_RPC_THROW(Rpc_pause, void, pause,
	                 GENODE_TYPE_LIST(Nonexistent_subject), Subject_id);
	GENODE_RPC_THROW(Rpc_resume, void, resume,
	                 GENODE_TYPE_LIST(Nonexistent_subject, Source_is_dead),
	                 Subject_id);
	GENODE_RPC_THROW(Rpc_subjects, size_t, subjects,
	                 GENODE_TYPE_LIST(Out_of_metadata));
	GENODE_RPC_THROW(Rpc_subject_info, Subject_info, subject_info,
	                 GENODE_TYPE_LIST(Nonexistent_subject), Subject_id);
	GENODE_RPC_THROW(Rpc_buffer, Dataspace_capability, buffer,
	                 GENODE_TYPE_LIST(Nonexistent_subject, Subject_not_traced),
	                 Subject_id);
	GENODE_RPC_THROW(Rpc_free, void, free,
	                 GENODE_TYPE_LIST(Nonexistent_subject), Subject_id);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_alloc_policy, Rpc_policy,
	                     Rpc_unload_policy, Rpc_trace, Rpc_rule, Rpc_pause,
	                     Rpc_resume, Rpc_subjects, Rpc_subject_info, Rpc_buffer,
	                     Rpc_free);
};

#endif /* _INCLUDE__TRACE_SESSION__TRACE_SESSION_H_ */
