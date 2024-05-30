/*
 * \brief  Dummy server interface
 * \author Alexander Boettcher
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

/* Genode includes */
#include <base/env.h>
#include <base/thread.h>
#include <base/rpc_client.h>
#include <base/rpc_server.h>

/* NOVA includes */
#include <nova/cap_map.h>
#include <nova/capability_space.h>

namespace Test {
	struct Session;
	struct Client;
	struct Component;

	long cap_void_manual(Genode::Native_capability dst,
	                     Genode::Native_capability arg1,
	                     Genode::addr_t &local_name);
}

/**
 * Test session interface definition
 */
struct Test::Session : Genode::Session
{
	static const char *service_name() { return "TEST"; }

	enum { CAP_QUOTA = 2 };

	GENODE_RPC(Rpc_cap_void, bool, cap_void, Genode::Native_capability,
	           Genode::addr_t &);
	GENODE_RPC(Rpc_void_cap, Genode::Native_capability,
	           void_cap);
	GENODE_RPC(Rpc_cap_cap, Genode::Native_capability, cap_cap,
	           Genode::addr_t);
	GENODE_RPC_INTERFACE(Rpc_cap_void, Rpc_void_cap, Rpc_cap_cap);
};

struct Test::Client : Genode::Rpc_client<Session>
{
	Client(Genode::Capability<Session> cap) : Rpc_client<Session>(cap) { }

	bool cap_void(Genode::Native_capability cap, Genode::addr_t &local_name) {
		return call<Rpc_cap_void>(cap, local_name); }

	Genode::Native_capability void_cap() {
		return call<Rpc_void_cap>(); }

	Genode::Native_capability cap_cap(Genode::addr_t cap) {
		return call<Rpc_cap_cap>(cap); }
};

struct Test::Component : Genode::Rpc_object<Test::Session, Test::Component>
{
	/* Test to transfer a object capability during send */
	bool cap_void(Genode::Native_capability, Genode::addr_t &);
	/* Test to transfer a object capability during reply */
	Genode::Native_capability void_cap();
	/* Test to transfer a specific object capability during reply */
	Genode::Native_capability cap_cap(Genode::addr_t);
};

namespace Test { typedef Genode::Capability<Test::Session> Capability; }

/**
 * Session implementation
 */
inline bool Test::Component::cap_void(Genode::Native_capability got_cap,
                                      Genode::addr_t &local_name)
{
	local_name = got_cap.local_name();

	if (!got_cap.valid())
		return false;

	/* be evil and keep this cap by manually incrementing the ref count */
	Genode::Cap_index idx(Genode::cap_map().find(got_cap.local_name()));
	idx.inc();

	return true;
}

inline Genode::Native_capability Test::Component::void_cap() {
	Genode::Native_capability send_cap = cap();

	/* XXX this code does does no longer work since the removal of 'solely_map' */
#if 0
	/* be evil and switch translation off - client ever uses a new selector */
	send_cap.solely_map();
#endif

	return send_cap;
}

inline Genode::Native_capability Test::Component::cap_cap(Genode::addr_t cap) {
	return Genode::Capability_space::import(cap); }
