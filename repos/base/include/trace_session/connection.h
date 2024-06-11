/*
 * \brief  Connection to TRACE service
 * \author Norman Feske
 * \date   2013-08-11
 */

/*
 * Copyright (C) 2013-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__TRACE_SESSION__CONNECTION_H_
#define _INCLUDE__TRACE_SESSION__CONNECTION_H_

#include <util/retry.h>
#include <trace_session/trace_session.h>
#include <base/rpc_client.h>
#include <base/connection.h>
#include <base/attached_dataspace.h>

namespace Genode { namespace Trace { struct Connection; } }


struct Genode::Trace::Connection : Genode::Connection<Genode::Trace::Session>,
                                   Genode::Rpc_client<Genode::Trace::Session>
{
	/**
	 * Shared-memory buffer used for carrying the payload of subject infos
	 */
	Attached_dataspace _argument_buffer;

	size_t const _max_arg_size;

	template <typename ERROR>
	auto _retry(auto const &fn) -> decltype(fn())
	{
		for (;;) {
			bool retry = false;
			auto const result = fn();
			if (result == ERROR::OUT_OF_CAPS) { upgrade_caps(2);     retry = true; }
			if (result == ERROR::OUT_OF_RAM)  { upgrade_ram(8*1024); retry = true; }

			if (!retry)
				return result;
		}
	}

	/**
	 * Constructor
	 *
	 * \param ram_quota        RAM donated for tracing purposes
	 * \param arg_buffer_size  session argument-buffer size
	 */
	Connection(Env &env, size_t ram_quota, size_t arg_buffer_size)
	:
		Genode::Connection<Session>(env, Label(), Ram_quota { 10*1024 + ram_quota },
		                            Args("arg_buffer_size=", arg_buffer_size)),
		Genode::Rpc_client<Session>(cap()),
		_argument_buffer(env.rm(), call<Rpc_dataspace>()),
		_max_arg_size(arg_buffer_size)
	{ }

	enum class Alloc_policy_error { INVALID };

	using Alloc_policy_result = Attempt<Policy_id, Alloc_policy_error>;

	/**
	 * Allocate policy-module backing store
	 */
	Alloc_policy_result alloc_policy(Policy_size size)
	{
		if (size.num_bytes > _max_arg_size)
			return Alloc_policy_error::INVALID;

		Alloc_policy_rpc_result const result = _retry<Alloc_policy_rpc_error>([&] {
			return call<Rpc_alloc_policy>(size); });

		return result.convert<Alloc_policy_result>(
			[&] (Policy_id const id)     { return id; },
			[&] (Alloc_policy_rpc_error) { return Alloc_policy_error::INVALID; });
	}

	/**
	 * Request policy-module backing store
	 *
	 * \return dataspace capability, or invalid capability if ID does not
	 *         refer to a known policy
	 */
	Dataspace_capability policy(Policy_id id) { return call<Rpc_policy>(id); }

	/**
	 * Remove a policy module from the TRACE service
	 */
	void unload_policy(Policy_id id) { call<Rpc_unload_policy>(id); }

	enum class Trace_error { FOREIGN, SOURCE_IS_DEAD, INVALID_SUBJECT, INVALID_POLICY };
	using Trace_result = Attempt<Trace_ok, Trace_error>;

	/**
	 * Start tracing of a subject
	 */
	Trace_result trace(Subject_id const s, Policy_id const p, Buffer_size const size)
	{
		Trace_rpc_result const rpc_result =
			_retry<Trace_rpc_error>([&] () -> Trace_rpc_result {
				return call<Rpc_trace>(s, p, size); });

		return rpc_result.convert<Trace_result>(
			[&] (Trace_ok ok) { return ok; },
			[&] (Trace_rpc_error e) {
				switch (e) {
				case Trace_rpc_error::OUT_OF_RAM: /* cannot occur, handled by '_retry' above */
				case Trace_rpc_error::OUT_OF_CAPS:     break;
				case Trace_rpc_error::FOREIGN:         return Trace_error::FOREIGN;
				case Trace_rpc_error::SOURCE_IS_DEAD:  return Trace_error::SOURCE_IS_DEAD;
				case Trace_rpc_error::INVALID_SUBJECT: return Trace_error::INVALID_SUBJECT;
				case Trace_rpc_error::INVALID_POLICY:  break;
				}
				return Trace_error::INVALID_POLICY;
			});
	}

	/**
	 * Retrieve subject directory
	 */
	Num_subjects subjects(Subject_id * const dst, Num_subjects const dst_num_subjects)
	{
		Subjects_rpc_result const result = _retry<Alloc_rpc_error>([&] {
			return call<Rpc_subjects>(); });

		return result.convert<Num_subjects>(

			[&] (Num_subjects const num_subjects) {
				auto const n = min(num_subjects.value, dst_num_subjects.value);
				memcpy(dst, _argument_buffer.local_addr<char>(), n*sizeof(Subject_id));
				return Num_subjects { n };
			},

			[&] (Alloc_rpc_error) { return Num_subjects { 0 }; });
	}

	struct For_each_subject_info_result { unsigned count; unsigned limit; };

	/**
	 * Call 'fn' for each trace subject with 'Subject_info' as argument
	 */
	For_each_subject_info_result for_each_subject_info(auto const &fn)
	{
		Infos_rpc_result const result = _retry<Alloc_rpc_error>([&] {
			return call<Rpc_subject_infos>(); });

		return result.convert<For_each_subject_info_result>(
			[&] (Num_subjects const n) -> For_each_subject_info_result {

				size_t const subject_bytes = sizeof(Subject_info) + sizeof(Subject_id);

				unsigned const max_subjects = unsigned(_argument_buffer.size() / subject_bytes);

				Subject_info * const infos = _argument_buffer.local_addr<Subject_info>();
				Subject_id   * const ids   = reinterpret_cast<Subject_id *>(infos + max_subjects);

				for (unsigned i = 0; i < n.value; i++)
					fn(ids[i], infos[i]);

				return { .count = n.value, .limit = max_subjects };
			},

			[&] (Alloc_rpc_error) { return For_each_subject_info_result { }; });
	}

	/**
	 * Release subject and free buffers
	 *
	 * If the source still exists, the buffers are freed but the subject
	 * stays intact.
	 */
	void free(Subject_id id) { call<Rpc_free>(id); }

	/**
	 * Pause generation of tracing data
	 */
	void pause(Subject_id id) { call<Rpc_pause>(id); }

	/**
	 * Resume generation of tracing data
	 */
	void resume(Subject_id id) { call<Rpc_resume>(id); }

	/**
	 * Obtain trace buffer of given subject
	 */
	Dataspace_capability buffer(Subject_id id) { return call<Rpc_buffer>(id); }
};

#endif /* _INCLUDE__TRACE_SESSION__CONNECTION_H_ */
