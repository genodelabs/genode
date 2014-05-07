/*
 * \brief  Client-side parent interface
 * \author Norman Feske
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__PARENT__CLIENT_H_
#define _INCLUDE__PARENT__CLIENT_H_

#include <parent/capability.h>
#include <base/rpc_client.h>

namespace Genode {

	struct Parent_client : Rpc_client<Parent>
	{
		explicit Parent_client(Parent_capability parent)
		: Rpc_client<Parent>(parent) { }

		void exit(int exit_value) { call<Rpc_exit>(exit_value); }

		void announce(Service_name const &service, Root_capability root) {
			call<Rpc_announce>(service, root); }

		Session_capability session(Service_name const &service,
		                           Session_args const &args,
		                           Affinity     const &affinity) {
			return call<Rpc_session>(service, args, affinity); }

		void upgrade(Session_capability to_session, Upgrade_args const &args) {
			call<Rpc_upgrade>(to_session, args); }

		void close(Session_capability session) { call<Rpc_close>(session); }

		Thread_capability main_thread_cap() const {
			return call<Rpc_main_thread>(); }

		void resource_avail_sigh(Signal_context_capability sigh) {
			call<Rpc_resource_avail_sigh>(sigh); }

		void resource_request(Resource_args const &args) {
			call<Rpc_resource_request>(args); }

		void yield_sigh(Signal_context_capability sigh) {
			call<Rpc_yield_sigh>(sigh); }

		Resource_args yield_request() { return call<Rpc_yield_request>(); }

		void yield_response() { call<Rpc_yield_response>(); }
	};
}

#endif /* _INCLUDE__PARENT__CLIENT_H_ */
