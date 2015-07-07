/*
 * \brief  Client-side TRACE session interface
 * \author Norman Feske
 * \date   2013-08-12
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__TRACE_SESSION__CLIENT_H_
#define _INCLUDE__TRACE_SESSION__CLIENT_H_

#include <trace_session/trace_session.h>
#include <base/rpc_client.h>
#include <base/env.h>

namespace Genode { namespace Trace { struct Session_client; } }


struct Genode::Trace::Session_client : Genode::Rpc_client<Genode::Trace::Session>
{
	private:

		/**
		 * Shared-memory buffer used for carrying the payload of the
		 * 'subjects()' RPC function.
		 */
		struct Argument_buffer
		{
			char   *base;
			size_t  size;

			Argument_buffer(Dataspace_capability ds)
			:
				base(env()->rm_session()->attach(ds)),
				size(ds.call<Dataspace::Rpc_size>())
			{ }

			~Argument_buffer()
			{
				env()->rm_session()->detach(base);
			}
		};

		Argument_buffer _argument_buffer;

	public:

		/**
		 * Constructor
		 */
		explicit Session_client(Capability<Trace::Session> session)
		:
			Rpc_client<Trace::Session>(session),
			_argument_buffer(call<Rpc_dataspace>())
		{ }

		/**
		 * Retrieve subject directory
		 *
		 * \throw Out_of_metadata
		 */
		size_t subjects(Subject_id *dst, size_t dst_len)
		{
			size_t const num_subjects = min(call<Rpc_subjects>(), dst_len);

			memcpy(dst, _argument_buffer.base, dst_len*sizeof(Subject_id));

			return num_subjects;
		}

		Policy_id alloc_policy(size_t size) override {
			return call<Rpc_alloc_policy>(size); }

		Dataspace_capability policy(Policy_id policy_id) override {
			return call<Rpc_policy>(policy_id); }

		void unload_policy(Policy_id policy_id) override {
			call<Rpc_unload_policy>(policy_id); }

		void trace(Subject_id s, Policy_id p, size_t buffer_size) override {
			call<Rpc_trace>(s, p, buffer_size); }

		void rule(Session_label const &label, Thread_name const &thread,
		          Policy_id policy, size_t buffer_size) override {
			call<Rpc_rule>(label, thread, policy, buffer_size); }

		void pause(Subject_id subject) override {
			call<Rpc_pause>(subject); }

		void resume(Subject_id subject) override {
			call<Rpc_resume>(subject); }

		Subject_info subject_info(Subject_id subject) override {
			return call<Rpc_subject_info>(subject); }

		Dataspace_capability buffer(Subject_id subject) override {
			return call<Rpc_buffer>(subject); }

		void free(Subject_id subject) override {
			call<Rpc_free>(subject); }
};

#endif /* _INCLUDE__TRACE_SESSION__CLIENT_H_ */
