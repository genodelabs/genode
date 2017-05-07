/*
 * \brief  Connection to file-system service
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_

#include <file_system_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>
#include <util/retry.h>

namespace File_system {

	struct Connection_base;
	struct Connection;

	/* recommended packet transmission buffer size */
	enum { DEFAULT_TX_BUF_SIZE = 128*1024 };

}


/**
 * The base implementation of a File_system connection
 */
struct File_system::Connection_base : Genode::Connection<Session>, Session_client
{
	/**
	 * Issue session request
	 *
	 * \noapi
	 */
	Capability<File_system::Session> _session(Genode::Parent &parent,
	                                          char     const *label,
	                                          char     const *root,
	                                          bool            writeable,
	                                          size_t          tx_buf_size)
	{
		return session(parent,
		               "ram_quota=%ld, "
		               "cap_quota=%ld, "
		               "tx_buf_size=%ld, "
		               "label=\"%s\", "
		               "root=\"%s\", "
		               "writeable=%d",
		               8*1024*sizeof(long) + tx_buf_size,
		               CAP_QUOTA,
		               tx_buf_size,
		               label, root, writeable);
	}

	/**
	 * Constructor
	 *
	 * \param tx_buffer_alloc  allocator used for managing the
	 *                         transmission buffer
	 * \param label            session label
	 * \param root             root directory of session
	 * \param writeable        session is writable
	 * \param tx_buf_size      size of transmission buffer in bytes
	 */
	Connection_base(Genode::Env             &env,
	                Genode::Range_allocator &tx_block_alloc,
	                char const              *label       = "",
	                char const              *root        = "/",
	                bool                     writeable   = true,
	                size_t                   tx_buf_size = DEFAULT_TX_BUF_SIZE)
	:
		Genode::Connection<Session>(env, _session(env.parent(), label, root,
		                                          writeable, tx_buf_size)),
		Session_client(cap(), tx_block_alloc, env.rm())
	{ }

	/**
	 * Constructor
	 *
	 * \noapi
	 * \deprecated  Use the constructor with 'Env &' as first
	 *              argument instead
	 */
	Connection_base(Genode::Range_allocator &tx_block_alloc,
	                size_t                   tx_buf_size = DEFAULT_TX_BUF_SIZE,
	                char const              *label       = "",
	                char const              *root        = "/",
	                bool                     writeable   = true) __attribute__((deprecated))
	:
		Genode::Connection<Session>(_session(*Genode::env_deprecated()->parent(), label,
		                                     root, writeable, tx_buf_size)),
		Session_client(cap(), tx_block_alloc, *Genode::env_deprecated()->rm_session())
	{ }
};


/**
 * A File_system connection that upgrades its RAM quota
 */
struct File_system::Connection : File_system::Connection_base
{
	/* reuse constructor */
	using Connection_base::Connection_base;

	/**
	 * Extend session quota on demand while calling an RPC function
	 *
	 * \noapi
	 */
	template <typename FUNC>
	auto _retry(FUNC func) -> decltype(func())
	{
		enum { UPGRADE_ATTEMPTS = 2 };
		return Genode::retry<Out_of_ram>(
			[&] () {
				return Genode::retry<Out_of_caps>(
					[&] () { return func(); },
					[&] () { File_system::Connection_base::upgrade_caps(2); },
					UPGRADE_ATTEMPTS);
			},
			[&] () { File_system::Connection_base::upgrade_ram(8*1024); },
			UPGRADE_ATTEMPTS);
	}

	Dir_handle dir(Path const &path, bool create) override
	{
		return _retry([&] () {
			return Session_client::dir(path, create); });
	}

	File_handle file(Dir_handle dir, Name const &name, Mode mode, bool create) override
	{
		return _retry([&] () {
			return Session_client::file(dir, name, mode, create); });
	}

	Symlink_handle symlink(Dir_handle dir, Name const &name, bool create) override
	{
		return _retry([&] () {
			return Session_client::symlink(dir, name, create); });
	}

	Node_handle node(Path const &path) override
	{
		return _retry([&] () {
			return Session_client::node(path); });
	}
};

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_ */
