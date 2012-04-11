/*
 * \brief  Client-side file-system session interface
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__CLIENT_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__CLIENT_H_

#include <base/rpc_client.h>
#include <file_system_session/capability.h>
#include <packet_stream_tx/client.h>

namespace File_system {

	class Session_client : public Rpc_client<Session>
	{
		private:

			Packet_stream_tx::Client<Tx> _tx;

		public:

			/**
			 * Constructor
			 *
			 * \param session          session capability
			 * \param tx_buffer_alloc  allocator used for managing the
			 *                         transmission buffer
			 */
			Session_client(Session_capability  session,
			               Range_allocator    &tx_buffer_alloc)
			:
				Rpc_client<Session>(session),
				_tx(call<Rpc_tx_cap>(), &tx_buffer_alloc)
			{ }


			/***********************************
			 ** File-system session interface **
			 ***********************************/

			Tx::Source *tx() { return _tx.source(); }

			File_handle file(Dir_handle dir, Name const &name, Mode mode, bool create)
			{
				return call<Rpc_file>(dir, name, mode, create);
			}

			Symlink_handle symlink(Dir_handle dir, Name const &name, bool create)
			{
				return call<Rpc_symlink>(dir, name, create);
			}

			Dir_handle dir(Path const &path, bool create)
			{
				return call<Rpc_dir>(path, create);
			}

			Node_handle node(Path const &path)
			{
				return call<Rpc_node>(path);
			}

			void close(Node_handle node)
			{
				call<Rpc_close>(node);
			}

			Status status(Node_handle node)
			{
				return call<Rpc_status>(node);
			}

			void control(Node_handle node, Control control)
			{
				call<Rpc_control>(node, control);
			}

			void unlink(Dir_handle dir, Name const &name)
			{
				call<Rpc_unlink>(dir, name);
			}

			void truncate(File_handle file, file_size_t size)
			{
				call<Rpc_truncate>(file, size);
			}

			void move(Dir_handle from_dir, Name const &from_name,
			          Dir_handle to_dir,   Name const &to_name)
			{
				call<Rpc_move>(from_dir, from_name, to_dir, to_name);
			}

	};
}

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__CLIENT_H_ */
