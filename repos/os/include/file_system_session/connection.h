/*
 * \brief  Connection to file-system service
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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
	 * Constructor
	 *
	 * \param tx_buffer_alloc  allocator used for managing the
	 *                         transmission buffer
	 * \param tx_buf_size      size of transmission buffer in bytes
	 * \param label            session label
	 * \param root             root directory of session
	 * \param writeable        session is writable
	 */
	Connection_base(Genode::Range_allocator &tx_block_alloc,
	                size_t                   tx_buf_size = DEFAULT_TX_BUF_SIZE,
	                char const              *label       = "",
	                char const              *root        = "/",
	                bool                     writeable   = true)
	:
		Genode::Connection<Session>(
			session("ram_quota=%zd, "
			        "tx_buf_size=%zd, "
			        "label=\"%s\", "
			        "root=\"%s\", "
			        "writeable=%d",
			        8*1024*sizeof(long) + tx_buf_size,
			        tx_buf_size,
			        label, root, writeable)),
		Session_client(cap(), tx_block_alloc)
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
	 * Upgrade the session quota in response to Out_of_metadata
	 */
	void upgrade_ram()
	{
		PWRN("upgrading File_system session");
		Genode::env()->parent()->upgrade(cap(), "ram=8K");
	}

	enum { UPGRADE_ATTEMPTS = 2 };

	Dir_handle dir(Path const &path, bool create) override
	{
		return Genode::retry<Out_of_metadata>(
			[&] () { return Session_client::dir(path, create); },
			[&] () { upgrade_ram(); },
			UPGRADE_ATTEMPTS);
	}

	File_handle file(Dir_handle dir, Name const &name, Mode mode, bool create) override
	{
		return Genode::retry<Out_of_metadata>(
			[&] () { return Session_client::file(dir, name, mode, create); },
			[&] () { upgrade_ram(); },
			UPGRADE_ATTEMPTS);
	}

	Symlink_handle symlink(Dir_handle dir, Name const &name, bool create) override
	{
		return Genode::retry<Out_of_metadata>(
			[&] () { return Session_client::symlink(dir, name, create); },
			[&] () { upgrade_ram(); },
			UPGRADE_ATTEMPTS);
	}

	Node_handle node(Path const &path) override
	{
		return Genode::retry<Out_of_metadata>(
			[&] () { return Session_client::node(path); },
			[&] () { upgrade_ram(); },
			UPGRADE_ATTEMPTS);
	}
};

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__CONNECTION_H_ */
