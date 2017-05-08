/*
 * \brief  RAM session interface
 * \author Norman Feske
 * \date   2006-05-11
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RAM_SESSION__RAM_SESSION_H_
#define _INCLUDE__RAM_SESSION__RAM_SESSION_H_

#include <base/stdint.h>
#include <base/ram_allocator.h>
#include <dataspace/capability.h>
#include <ram_session/capability.h>
#include <session/session.h>

namespace Genode {
	struct Ram_session_client;
	struct Ram_session;
}


/**
 * RAM session interface
 */
struct Genode::Ram_session : Session, Ram_allocator
{
	static const char *service_name() { return "RAM"; }

	enum { CAP_QUOTA = 8 };

	typedef Ram_session_client Client;


	/*********************
	 ** Exception types **
	 *********************/

	class Invalid_session       : public Exception { };
	class Undefined_ref_account : public Exception { };

	/* deprecated */
	typedef Out_of_ram Quota_exceeded;

	/**
	 * Destructor
	 */
	virtual ~Ram_session() { }

	/**
	 * Define reference account for the RAM session
	 *
	 * \param   ram_session    reference account
	 *
	 * \throw Invalid_session
	 *
	 * Each RAM session requires another RAM session as reference
	 * account to transfer quota to and from. The reference account can
	 * be defined only once.
	 */
	virtual void ref_account(Ram_session_capability ram_session) = 0;

	/**
	 * Transfer quota to another RAM session
	 *
	 * \param ram_session  receiver of quota donation
	 * \param amount       amount of quota to donate
	 *
	 * \throw Out_of_ram
	 * \throw Invalid_session
	 * \throw Undefined_ref_account
	 *
	 * Quota can only be transfered if the specified RAM session is
	 * either the reference account for this session or vice versa.
	 */
	virtual void transfer_quota(Ram_session_capability ram_session, Ram_quota amount) = 0;

	/**
	 * Return current quota limit
	 */
	virtual Ram_quota ram_quota() const = 0;

	/**
	 * Return used quota
	 */
	virtual Ram_quota used_ram() const = 0;

	/**
	 * Return amount of available quota
	 */
	Ram_quota avail_ram() const { return { ram_quota().value - used_ram().value }; }


	/*********************
	 ** RPC declaration **
	 *********************/

	GENODE_RPC_THROW(Rpc_alloc, Ram_dataspace_capability, alloc,
	                 GENODE_TYPE_LIST(Out_of_ram, Out_of_caps, Undefined_ref_account),
	                 size_t, Cache_attribute);
	GENODE_RPC(Rpc_free, void, free, Ram_dataspace_capability);
	GENODE_RPC(Rpc_ref_account, void, ref_account, Capability<Ram_session>);
	GENODE_RPC_THROW(Rpc_transfer_ram_quota, void, transfer_quota,
	                 GENODE_TYPE_LIST(Out_of_ram, Invalid_session, Undefined_ref_account),
	                 Capability<Ram_session>, Ram_quota);
	GENODE_RPC(Rpc_ram_quota, Ram_quota, ram_quota);
	GENODE_RPC(Rpc_used_ram, Ram_quota, used_ram);

	GENODE_RPC_INTERFACE(Rpc_alloc, Rpc_free, Rpc_ref_account,
	                     Rpc_transfer_ram_quota, Rpc_ram_quota, Rpc_used_ram);
};

#endif /* _INCLUDE__RAM_SESSION__RAM_SESSION_H_ */
