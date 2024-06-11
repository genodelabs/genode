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

#include <util/attempt.h>
#include <base/trace/types.h>
#include <dataspace/capability.h>
#include <session/session.h>

namespace Genode { namespace Trace { struct Session; } }


struct Genode::Trace::Session : Genode::Session
{
	/**
	 * \noapi
	 */
	static const char *service_name() { return "TRACE"; }

	enum { CAP_QUOTA = 6 };

	enum class Alloc_rpc_error        { OUT_OF_RAM, OUT_OF_CAPS };
	enum class Alloc_policy_rpc_error { OUT_OF_RAM, OUT_OF_CAPS, INVALID };
	enum class Trace_rpc_error        { OUT_OF_RAM, OUT_OF_CAPS, FOREIGN,
	                                    SOURCE_IS_DEAD, INVALID_SUBJECT,
	                                    INVALID_POLICY };

	using Alloc_policy_rpc_result = Attempt<Policy_id,    Alloc_policy_rpc_error>;
	using Subjects_rpc_result     = Attempt<Num_subjects, Alloc_rpc_error>;
	using Infos_rpc_result        = Attempt<Num_subjects, Alloc_rpc_error>;
	using Trace_rpc_result        = Attempt<Trace_ok,     Trace_rpc_error>;


	virtual ~Session() { }


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC(Rpc_dataspace, Dataspace_capability, dataspace);
	GENODE_RPC(Rpc_alloc_policy, Alloc_policy_rpc_result, alloc_policy, Policy_size);
	GENODE_RPC(Rpc_policy, Dataspace_capability, policy, Policy_id);
	GENODE_RPC(Rpc_unload_policy, void, unload_policy, Policy_id);
	GENODE_RPC(Rpc_trace, Trace_rpc_result, trace, Subject_id, Policy_id, Buffer_size);
	GENODE_RPC(Rpc_pause, void, pause, Subject_id);
	GENODE_RPC(Rpc_resume, void, resume, Subject_id);
	GENODE_RPC(Rpc_subjects, Subjects_rpc_result, subjects);
	GENODE_RPC(Rpc_subject_infos, Infos_rpc_result, subject_infos);
	GENODE_RPC(Rpc_buffer, Dataspace_capability, buffer, Subject_id);
	GENODE_RPC(Rpc_free, void, free, Subject_id);

	GENODE_RPC_INTERFACE(Rpc_dataspace, Rpc_alloc_policy, Rpc_policy,
	                     Rpc_unload_policy, Rpc_trace, Rpc_pause,
	                     Rpc_resume, Rpc_subjects, Rpc_buffer,
	                     Rpc_free, Rpc_subject_infos);
};

#endif /* _INCLUDE__TRACE_SESSION__TRACE_SESSION_H_ */
