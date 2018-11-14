/*
 * \brief  Client-side parent interface
 * \author Norman Feske
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PARENT__CLIENT_H_
#define _INCLUDE__PARENT__CLIENT_H_

#include <parent/capability.h>
#include <base/rpc_client.h>

namespace Genode { struct Parent_client; }


struct Genode::Parent_client : Rpc_client<Parent>
{
	explicit Parent_client(Parent_capability parent)
	: Rpc_client<Parent>(parent) { }

	void exit(int exit_value) override { call<Rpc_exit>(exit_value); }

	void announce(Service_name const &service) override {
		call<Rpc_announce>(service); }

	void session_sigh(Signal_context_capability sigh) override {
		call<Rpc_session_sigh>(sigh); }

	Session_capability session(Client::Id          id,
	                           Service_name const &service,
	                           Session_args const &args,
	                           Affinity     const &affinity) override {
		return call<Rpc_session>(id, service, args, affinity); }

	Session_capability session_cap(Client::Id id) override {
		return call<Rpc_session_cap>(id); }

	Upgrade_result upgrade(Client::Id to_session, Upgrade_args const &args) override {
		return call<Rpc_upgrade>(to_session, args); }

	Close_result close(Client::Id id) override { return call<Rpc_close>(id); }

	void session_response(Id_space<Server>::Id id, Session_response response) override {
		call<Rpc_session_response>(id, response); }

	void deliver_session_cap(Id_space<Server>::Id id,
	                         Session_capability cap) override {
		call<Rpc_deliver_session_cap>(id, cap); }

	Thread_capability main_thread_cap() const override {
		return call<Rpc_main_thread>(); }

	void resource_avail_sigh(Signal_context_capability sigh) override {
		call<Rpc_resource_avail_sigh>(sigh); }

	void resource_request(Resource_args const &args) override {
		call<Rpc_resource_request>(args); }

	void yield_sigh(Signal_context_capability sigh) override {
		call<Rpc_yield_sigh>(sigh); }

	Resource_args yield_request() override { return call<Rpc_yield_request>(); }

	void yield_response() override { call<Rpc_yield_response>(); }

	void heartbeat_sigh(Signal_context_capability sigh) override {
		call<Rpc_heartbeat_sigh>(sigh); }

	void heartbeat_response() override { call<Rpc_heartbeat_response>(); }
};

#endif /* _INCLUDE__PARENT__CLIENT_H_ */
