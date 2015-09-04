/*
 * \brief  VFS File_system server
 * \author Emery Hemingway
 * \date   2015-08-16
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <file_system/node.h>
#include <vfs/dir_file_system.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <vfs/file_system_factory.h>
#include <os/config.h>

/* Local includes */
#include "assert.h"
#include "node_cache.h"
#include "node_handle_registry.h"

namespace File_system {

	using namespace Genode;
	using namespace Vfs;

	class  Session_component;
	class  Root;
	struct Main;

	typedef Genode::Path<MAX_PATH_LEN> Subpath;

	Vfs::File_system *root()
	{
		static Vfs::Dir_file_system _root(config()->xml_node().sub_node("vfs"),
		                                  Vfs::global_file_system_factory());

		return &_root;
	}
};


class File_system::Session_component : public Session_rpc_object
{
	private:

		Node_handle_registry                  _handle_registry;
		Subpath                         const _root_path;
		Signal_rpc_member<Session_component>  _process_packet_dispatcher;
		bool                                  _writable;


		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 */
		void _process_packet_op(Packet_descriptor &packet)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const offset  = packet.position();

			if ((!(content && length)) || (packet.length() > packet.size())) {
				packet.succeeded(false);
				return;
			}

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ: {
				Node *node = _handle_registry.lookup_read(packet.handle());
				if (!node) return;
				Node_lock_guard guard(node);
				res_length = node->read((char *)content, length, offset);
				break;
			}

			case Packet_descriptor::WRITE: {
				Node *node = _handle_registry.lookup_write(packet.handle());
				if (!node) return;
				Node_lock_guard guard(node);
				res_length = node->write((char const *)content, length, offset);
				break;
			}
			}

			if (res_length) {
				packet.length(res_length);
				packet.succeeded(true);
			}
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			_process_packet_op(packet);

			/*
			 * The 'acknowledge_packet' function cannot block because we
			 * checked for 'ready_to_ack' in '_process_packets'.
			 */
			tx_sink()->acknowledge_packet(packet);
		}

		/**
		 * Called by signal dispatcher, executed in the context of the main
		 * thread (not serialized with the RPC functions)
		 */
		void _process_packets(unsigned)
		{
			while (tx_sink()->packet_avail()) {

				/*
				 * Make sure that the '_process_packet' function does not
				 * block.
				 *
				 * If the acknowledgement queue is full, we defer packet
				 * processing until the client processed pending
				 * acknowledgements and thereby emitted a ready-to-ack
				 * signal. Otherwise, the call of 'acknowledge_packet()'
				 * in '_process_packet' would infinitely block the context
				 * of the main thread. The main thread is however needed
				 * for receiving any subsequent 'ready-to-ack' signals.
				 */
				if (!tx_sink()->ready_to_ack())
					return;

				_process_packet();
			}
		}

		/**
		 * Check if string represents a valid path (must start with '/')
		 */
		static void _assert_valid_path(char const *path) {
			if (!path || path[0] != '/') throw Lookup_failed(); }

		/**
		 * Check if string represents a valid name (must not contain '/')
		 */
		static void _assert_valid_name(char const *name) {
			for (char const *p = name; *p; ++p)
				if (*p == '/')
					throw Invalid_name(); }

	public:

		/**
		 * Constructor
		 * \param ep           thead entrypoint for session
		 * \param cache        node cache
		 * \param tx_buf_size  shared transmission buffer size
		 * \param root_path    path root of the session
		 * \param writable     whether the session can modify files
		 */
		Session_component(Server::Entrypoint &ep,
		                  Node_cache         &cache,
		                  size_t              tx_buf_size,
		                  Subpath      const &root_path,
		                  bool                writable)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_handle_registry(cache),
			_root_path(root_path.base()),
			_process_packet_dispatcher(ep, *this, &Session_component::_process_packets),
			_writable(writable)
		{
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(Path const &path, bool create) override
		{
			if ((!_writable) && create)
				throw Permission_denied();

			char const *path_str = path.string();
			_assert_valid_path(path_str);

			/* re-root the path */
			Subpath dir_path(path_str+1, _root_path.base());
			path_str = dir_path.base();

			return _handle_registry.directory(path_str, create);
		}

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode fs_mode, bool create) override
		{
			if ((!_writable) &&
				(create || (fs_mode != STAT_ONLY && fs_mode != READ_ONLY)))
					throw Permission_denied();

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			/* re-root the path */
			Subpath subpath(name_str, dir->path());
			char *path_str = subpath.base();
			File_handle handle = _handle_registry.file(path_str, fs_mode, create);
			if (create)
				dir->mark_as_updated();
			return handle;
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create) override
		{
			if (create && !_writable) throw Permission_denied();

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			/* re-root the path */
			Subpath subpath(name_str, dir->path());
			char *path_str = subpath.base();

			Symlink_handle handle = _handle_registry.symlink(
				path_str, (_writable ? READ_WRITE : READ_ONLY), create);
			if (create)
				dir->mark_as_updated();
			return handle;
		}

		Node_handle node(Path const &path) override
		{
			char const *path_str = path.string();
			_assert_valid_path(path_str);

			/* re-root the path */
			Subpath sub_path(path_str+1, _root_path.base());
			path_str = sub_path.base();

			return _handle_registry.node(path_str);
		}

		void close(Node_handle handle) { _handle_registry.free(handle); }

		Status status(Node_handle node_handle)
		{
			Directory_service::Stat vfs_stat;
			File_system::Status      fs_stat;
			Node *node;
			try { node = _handle_registry.lookup_and_lock(node_handle); }
			catch (Invalid_handle) { return fs_stat; }
			Node_lock_guard guard(node);

			char const *path_str = node->path();

			if (root()->stat(path_str, vfs_stat) != Directory_service::STAT_OK)
				return fs_stat;

			fs_stat.inode = vfs_stat.inode;

			switch (vfs_stat.mode & (
				Directory_service::STAT_MODE_DIRECTORY |
				Directory_service::STAT_MODE_SYMLINK |
				File_system::Status::MODE_FILE)) {

			case Directory_service::STAT_MODE_DIRECTORY:
				fs_stat.mode = File_system::Status::MODE_DIRECTORY;
				fs_stat.size = root()->num_dirent(path_str) * sizeof(Directory_entry);
				return fs_stat;

			case Directory_service::STAT_MODE_SYMLINK:
				fs_stat.mode = File_system::Status::MODE_SYMLINK;
				break;

			default: /* Directory_service::STAT_MODE_FILE */
				fs_stat.mode = File_system::Status::MODE_FILE;
				break;
			}

			fs_stat.size = vfs_stat.size;
			return fs_stat;
		}

		/**
		 * Set information about an open file or directory
		 */
		void control(Node_handle, Control) { }

		/**
		 * Delete file or directory
		 */
		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!_writable) throw Permission_denied();

			char const *str = name.string();
			_assert_valid_name(str);

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard guard(dir);

			Subpath path(str, dir->path());
			str = path.base();

			/*
			 * If the file is open in any other session,
			 * unlinking is not allowed. This is to prevent
			 * dangling pointers in handle registries.
			 */
			if (_handle_registry.is_open(str))
				throw Permission_denied();

			assert_unlink(root()->unlink(str));
			dir->mark_as_updated();
		}

		/**
		 * Truncate or grow file to specified size
		 */
		void truncate(File_handle file_handle, file_size_t size)
		{
			File *file = _handle_registry.lookup_and_lock(file_handle);
			Node_lock_guard guard(file);
			file->truncate(size);
			file->mark_as_updated();
		}

		/**
		 * Move and rename directory entry
		 */
		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle to_dir_handle,   Name const &to_name)
		{
			if (!_writable)
				throw Permission_denied();

			char const *from_str = from_name.string();
			char const   *to_str =   to_name.string();

			_assert_valid_name(from_str);
			_assert_valid_name(  to_str);

			if (_handle_registry.refer_to_same_node(from_dir_handle, to_dir_handle)) {
				Directory *dir = _handle_registry.lookup_and_lock(from_dir_handle);
				Node_lock_guard from_guard(dir);

				from_str = Subpath(from_str, dir->path()).base();
				  to_str = Subpath(  to_str, dir->path()).base();

				if (_handle_registry.is_open(to_str))
					throw Permission_denied();

				assert_rename(root()->rename(from_str, to_str));
				_handle_registry.rename(from_str, to_str);

				dir->mark_as_updated();
				return;
			}

			Directory *from_dir = _handle_registry.lookup_and_lock(from_dir_handle);
			Node_lock_guard from_guard(from_dir);

			Directory   *to_dir = _handle_registry.lookup_and_lock(  to_dir_handle);
			Node_lock_guard to_guard(to_dir);

			from_str = Subpath(from_str, from_dir->path()).base();
			  to_str = Subpath(  to_str,   to_dir->path()).base();
			if (_handle_registry.is_open(to_str))
				throw Permission_denied();

			assert_rename(root()->rename(from_str, to_str));
			_handle_registry.rename(from_str, to_str);

			from_dir->mark_as_updated();
			to_dir->mark_as_updated();
		}

		void sigh(Node_handle node_handle, Signal_context_capability sigh) {
			_handle_registry.sigh(node_handle, sigh); }

		/**
		 * Sync the VFS and send any pending signal on the node.
		 */
		void sync(Node_handle handle)
		{
			Node *node;
			try {
				node = _handle_registry.lookup_and_lock(handle);
			} catch (Invalid_handle) {
				return;
			}
			Node_lock_guard guard(node);

			root()->sync(node->path());
			node->notify_listeners();
		}
};


class File_system::Root : public Root_component<Session_component>
{
	private:

		Node_cache          _cache;
		Server::Entrypoint &_ep;

	protected:

		Session_component *_create_session(const char *args) override
		{
			Subpath session_root;
			bool writeable = false;

			Session_label const label(args);

			try {
				Session_policy policy(label);
				char buf[MAX_PATH_LEN];

				/* Determine the session root directory.
				 * Defaults to '/' if not specified by session
				 * policy or session arguments.
				 */
				try {
					policy.attribute("root").value(buf, MAX_PATH_LEN);
					session_root.append(buf);
				} catch (Xml_node::Nonexistent_attribute) { }

				Arg_string::find_arg(args, "root").string(buf, MAX_PATH_LEN, "/");
				session_root.append(buf);

				/* Determine if the session is writeable.
				 * Policy overrides arguments, both default to false.
				 */
				if (policy.attribute_value("writeable", false))
					writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			} catch (Session_policy::No_policy_defined) {
				PERR("rejecting session request, no matching policy for %s", label.string());
				throw Root::Unavailable();
			}

			size_t ram_quota   = Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size = Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			if (!tx_buf_size)
				throw Root::Invalid_args();

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((size_t)4096, session_size) > ram_quota) {
				PERR("insufficient 'ram_quota' from %s, got %zd, need %zd",
				     label.string(), ram_quota, session_size);
				throw Root::Quota_exceeded();
			}

			/* check if the session root exists */
			if (!root()->is_directory(session_root.base())) {
				PERR("session root '%s' not found for '%s'", session_root.base(), label.string());
				throw Root::Unavailable();
			}

			Session_component *session = new(md_alloc())
				Session_component(_ep, _cache, tx_buf_size, session_root, writeable);
			PLOG("session opened for '%s' at '%s'", label.string(), session_root.base());
			return session;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep        entrypoint
		 * \param md_alloc  meta-data allocator
		 */
		Root(Server::Entrypoint &ep, Allocator &md_alloc)
		:
			Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep)
		{ }
};


struct File_system::Main
{
	Server::Entrypoint &ep;

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root = { ep, sliced_heap };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(fs_root));
		PLOG("virtual file system server started");
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name() { return "vfs_ep"; }

Genode::size_t Server::stack_size() { return 2*1024*sizeof(long); }

void Server::construct(Server::Entrypoint &ep) {
	static File_system::Main inst(ep); }
