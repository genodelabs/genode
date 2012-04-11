/*
 * \brief  File-system session interface
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__FILE_SYSTEM_SESSION_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__FILE_SYSTEM_SESSION_H_

#include <base/exception.h>
#include <os/packet_stream.h>
#include <packet_stream_tx/packet_stream_tx.h>
#include <session/session.h>

namespace File_system {

	using namespace Genode;

	struct Node_handle
	{
		int value;

		Node_handle() : value(-1) { }
		Node_handle(int v) : value(v) { }
	};


	struct File_handle : Node_handle
	{
		File_handle() { }
		File_handle(int v) : Node_handle(v) { }
	};


	struct Dir_handle : Node_handle
	{
		Dir_handle() { }
		Dir_handle(int v) : Node_handle(v) { }
	};


	struct Symlink_handle : Node_handle
	{
		Symlink_handle() { }
		Symlink_handle(int v) : Node_handle(v) { }
	};


	/**
	 * Type of client context embedded in each packet descriptor
	 *
	 * Using the opaque refererence, the client is able to attribute incoming
	 * packet acknowledgements to a context that is meaningful for the client.
	 * It has no meaning at the server side.
	 */
	struct Packet_ref;


	typedef uint64_t seek_off_t;
	typedef uint64_t file_size_t;


	class Packet_descriptor : public ::Packet_descriptor
	{
		public:

			enum Opcode { READ, WRITE };

		private:

			Node_handle _handle;   /* node handle */
			Opcode      _op;       /* requested operation */
			seek_off_t  _position; /* seek offset in bytes */
			size_t      _length;   /* transaction length in bytes */
			bool        _success;  /* indicates success of operation */
			Packet_ref *_ref;      /* opaque reference used at the client side
			                          for recognizing acknowledgements */

		public:

			/**
			 * Constructor
			 */
			Packet_descriptor(off_t offset = 0, size_t size = 0)
			:
				::Packet_descriptor(offset, size), _handle(-1),
				_op(READ), _position(0), _length(0), _success(false) { }

			/**
			 * Constructor
			 *
			 * \param position  seek offset in bytes (by default, append)
			 */
			Packet_descriptor(Packet_descriptor p, Packet_ref *ref,
			                  Node_handle handle, Opcode op, size_t length,
			                  seek_off_t position = ~0)
			:
				::Packet_descriptor(p.offset(), p.size()),
				_handle(handle), _op(op),
				_position(position), _length(length), _success(false),
				_ref(ref)
			{ }

			Node_handle handle()    const { return _handle;   }
			Opcode      operation() const { return _op;       }
			seek_off_t  position()  const { return _position; }
			size_t      length()    const { return _length;   }
			bool        succeeded() const { return _success;  }
			Packet_ref *ref()       const { return _ref;      }

			/*
			 * Accessors called at the server side
			 */
			void succeeded(bool b) { _success = b ? 1 : 0; }
			void length(size_t length) { _length = length; }
	};


	/**
	 * Flags as supplied to 'file', 'dir', and 'symlink' calls
	 */
	enum Mode { STAT_ONLY = 0, READ_ONLY = 1, WRITE_ONLY = 2, READ_WRITE = 3 };

	enum { MAX_NAME_LEN = 128, MAX_PATH_LEN = 1024 };

	typedef Rpc_in_buffer<MAX_NAME_LEN> Name;
	typedef Rpc_in_buffer<MAX_PATH_LEN> Path;


	struct Status
	{
		enum {
			MODE_SYMLINK   = 0020000,
			MODE_FILE      = 0100000,
			MODE_DIRECTORY = 0040000,
		};

		/*
		 * XXX  add access time
		 * XXX  add executable bit
		 */

		file_size_t   size;
		unsigned      mode;
		unsigned long inode;

		bool is_directory() const { return mode & MODE_DIRECTORY; }
		bool is_symlink()   const { return mode & MODE_SYMLINK; }
	};


	struct Control { /* to manipulate the executable bit */ };


	/**
	 * Data structure returned when reading from a directory node
	 */
	struct Directory_entry
	{
		enum Type { TYPE_FILE, TYPE_DIRECTORY, TYPE_SYMLINK };
		Type type;
		char name[MAX_NAME_LEN];
	};


	/*
	 * Exception types
	 */
	class Exception           : public Genode::Exception { };
	class Permission_denied   : Exception { };
	class Node_already_exists : Exception { };
	class Lookup_failed       : Exception { };
	class Name_too_long       : Exception { };
	class No_space            : Exception { };
	class Out_of_node_handles : Exception { };
	class Invalid_handle      : Exception { };
	class Invalid_name        : Exception { };
	class Size_limit_reached  : Exception { };


	struct Session : public Genode::Session
	{
		enum { TX_QUEUE_SIZE = 16 };

		typedef Packet_stream_policy<File_system::Packet_descriptor,
		                             TX_QUEUE_SIZE, TX_QUEUE_SIZE,
		                             char> Tx_policy;

		typedef Packet_stream_tx::Channel<Tx_policy> Tx;

		static const char *service_name() { return "File_system"; }

		virtual ~Session() { }

		/**
		 * Request client-side packet-stream interface of tx channel
		 */
		virtual Tx::Source *tx() { return 0; }

		/**
		 * Open or create file
		 *
		 * \throw Invalid_handle       directory handle is invalid
		 * \throw Node_already_exists  file cannot be created because a
		 *                             node with the same name already exists
		 * \throw Invalid_name         file name contains invalid characters
		 * \throw Lookup_failed        the name refers to a node other than a
		 *                             file
		 */
		virtual File_handle file(Dir_handle, Name const &name, Mode, bool create) = 0;

		/**
		 * Open or create symlink
		 */
		virtual Symlink_handle symlink(Dir_handle, Name const &name, bool create) = 0;

		/**
		 * Open or create directory
		 *
		 * \throw Permission_denied
		 * \throw Node_already_exists  directory cannot be created because a
		 *                             node with the same name already exists
		 * \throw Lookup_failed        path lookup failed because one element
		 *                             of 'path' does not exist
		 * \throw Name_too_long
		 * \throw No_space
		 */
		virtual Dir_handle dir(Path const &path, bool create) = 0;

		/**
		 * Open existing node
		 *
		 * The returned node handle can be used merely as argument for
		 * 'status'.
		 */
		virtual Node_handle node(Path const &path) = 0;

		/**
		 * Close file
		 */
		virtual void close(Node_handle) = 0;

		/**
		 * Request information about an open file or directory
		 */
		virtual Status status(Node_handle) = 0;

		/**
		 * Set information about an open file or directory
		 */
		virtual void control(Node_handle, Control) = 0;

		/**
		 * Delete file or directory
		 */
		virtual void unlink(Dir_handle, Name const &) = 0;

		/**
		 * Truncate or grow file to specified size
		 */
		virtual void truncate(File_handle, file_size_t size) = 0;

		/**
		 * Move and rename directory entry
		 */
		virtual void move(Dir_handle, Name const &from,
		                  Dir_handle, Name const &to) = 0;


		/*******************
		 ** RPC interface **
		 *******************/

		GENODE_RPC(Rpc_tx_cap, Capability<Tx>, _tx_cap);
		GENODE_RPC_THROW(Rpc_file, File_handle, file,
		                 GENODE_TYPE_LIST(Invalid_handle, Node_already_exists,
		                                  Invalid_name, Lookup_failed,
		                                  Permission_denied),
		                 Dir_handle, Name const &, Mode, bool);
		GENODE_RPC_THROW(Rpc_symlink, Symlink_handle, symlink,
		                 GENODE_TYPE_LIST(Invalid_handle, Node_already_exists,
		                                  Invalid_name, Lookup_failed),
		                 Dir_handle, Name const &, bool);
		GENODE_RPC_THROW(Rpc_dir, Dir_handle, dir,
		                 GENODE_TYPE_LIST(Permission_denied, Node_already_exists,
		                                  Lookup_failed, Name_too_long, No_space),
		                 Path const &, bool);
		GENODE_RPC_THROW(Rpc_node, Node_handle, node,
		                 GENODE_TYPE_LIST(Lookup_failed),
		                 Path const &);
		GENODE_RPC(Rpc_close, void, close, Node_handle);
		GENODE_RPC(Rpc_status, Status, status, Node_handle);
		GENODE_RPC(Rpc_control, void, control, Node_handle, Control);
		GENODE_RPC_THROW(Rpc_unlink, void, unlink,
		                 GENODE_TYPE_LIST(Permission_denied, Invalid_name, Lookup_failed),
		                 Dir_handle, Name const &);
		GENODE_RPC_THROW(Rpc_truncate, void, truncate,
		                 GENODE_TYPE_LIST(Permission_denied, Invalid_handle),
		                 File_handle, file_size_t);
		GENODE_RPC_THROW(Rpc_move, void, move,
		                 GENODE_TYPE_LIST(Permission_denied, Invalid_name, Lookup_failed),
		                 Dir_handle, Name const &, Dir_handle, Name const &);

		/*
		 * Manual type-list definition, needed because the RPC interface
		 * exceeds the maximum number of type-list elements supported by
		 * 'Genode::Meta::Type_list<>'.
		 */
		typedef Meta::Type_tuple<Rpc_tx_cap,
		        Meta::Type_tuple<Rpc_file,
		        Meta::Type_tuple<Rpc_symlink,
		        Meta::Type_tuple<Rpc_dir,
		        Meta::Type_tuple<Rpc_node,
		        Meta::Type_tuple<Rpc_close,
		        Meta::Type_tuple<Rpc_status,
		        Meta::Type_tuple<Rpc_control,
		        Meta::Type_tuple<Rpc_unlink,
		        Meta::Type_tuple<Rpc_truncate,
		        Meta::Type_tuple<Rpc_move,
		                         Meta::Empty>
		        > > > > > > > > > > Rpc_functions;
	};
}

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__FILE_SYSTEM_SESSION_H_ */
