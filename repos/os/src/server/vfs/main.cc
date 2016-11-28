/*
 * \brief  VFS File_system server
 * \author Emery Hemingway
 * \date   2015-08-16
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system_session/rpc_object.h>
#include <base/heap.h>
#include <ram_session/connection.h>
#include <root/component.h>
#include <vfs/dir_file_system.h>
#include <os/session_policy.h>
#include <vfs/file_system_factory.h>
#include <os/config.h>
#include <base/sleep.h>
#include <base/component.h>

/* Local includes */
#include "assert.h"
#include "node.h"

namespace Vfs_server {

	using namespace File_system;
	using namespace Vfs;

	class  Session_component;
	class  Root;

	static Genode::Xml_node vfs_config()
	{
		try { return Genode::config()->xml_node().sub_node("vfs"); }
		catch (...) {
			Genode::error("VFS not configured");
			Genode::env()->parent()->exit(~0);
			Genode::sleep_forever();
		}
	}
};


class Vfs_server::Session_component :
	public File_system::Session_rpc_object
{
	private:

		/* maximum number of open nodes per session */
		enum { MAX_NODE_HANDLES = 128U };

		Node *_nodes[MAX_NODE_HANDLES];

		/**
		 * Each open node handle can act as a listener to be informed about
		 * node changes.
		 */
		Listener _listeners[MAX_NODE_HANDLES];

		Genode::String<160>     _label;

		Genode::Ram_connection  _ram = { _label.string() };
		Genode::Heap            _alloc =
			{ &_ram, Genode::env()->rm_session() };

		Genode::Signal_handler<Session_component>
		                        _process_packet_dispatcher;
		Vfs::Dir_file_system   &_vfs;
		Directory               _root;
		bool                    _writable;


		/****************************
		 ** Handle to node mapping **
		 ****************************/

		bool _in_range(int handle) const {
			return ((handle >= 0) && (handle < MAX_NODE_HANDLES));
		}

		int _next_slot()
		{
			for (int i = 1; i < MAX_NODE_HANDLES; ++i)
				if (_nodes[i] == nullptr)
					return i;

			throw Out_of_metadata();
		}

		/**
		 * Lookup node using its handle as key
		 */
		Node *_lookup_node(Node_handle handle) {
			return _in_range(handle.value) ? _nodes[handle.value] : 0; }

		/**
		 * Lookup typed node using its handle as key
		 *
		 * \throw Invalid_handle
		 */
		template <typename HANDLE_TYPE>
		typename Node_type<HANDLE_TYPE>::Type &_lookup(HANDLE_TYPE handle)
		{
			if (!_in_range(handle.value))
				throw Invalid_handle();

			typedef typename Node_type<HANDLE_TYPE>::Type Node;
			Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
			if (!node)
				throw Invalid_handle();

			return *node;
		}

		bool _refer_to_same_node(Node_handle h1, Node_handle h2) const
		{
			if (!(_in_range(h1.value) && _in_range(h2.value)))
				throw Invalid_handle();

			return _nodes[h1.value] == _nodes[h2.value];
		}


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
			seek_off_t const seek    = packet.position();

			/* assume failure by default */
			packet.succeeded(false);

			if ((!(content && length)) || (packet.length() > packet.size())) {
				return;
			}

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ: {
				Node *node = _lookup_node(packet.handle());
				if (!(node && (node->mode&READ_ONLY)))
					return;

				res_length = node->read(_vfs, (char *)content, length, seek);
				break;
			}

			case Packet_descriptor::WRITE: {
				Node *node = _lookup_node(packet.handle());
				if (!(node && (node->mode&WRITE_ONLY)))
					return;

				res_length = node->write(_vfs, (char const *)content, length, seek);
				break;
			}
			}

			packet.length(res_length);
			packet.succeeded(!!res_length);
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

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
		void _process_packets()
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
		static void _assert_valid_name(char const *name)
		{
			if (!*name) throw Invalid_name();
			for (char const *p = name; *p; ++p)
				if (*p == '/')
					throw Invalid_name();
		}

	public:

		/**
		 * Constructor
		 * \param ep           thead entrypoint for session
		 * \param cache        node cache
		 * \param tx_buf_size  shared transmission buffer size
		 * \param root_path    path root of the session
		 * \param writable     whether the session can modify files
		 */

		Session_component(Genode::Env         &env,
		                  char          const *label,
		                  size_t               ram_quota,
		                  size_t               tx_buf_size,
		                  Vfs::Dir_file_system &vfs,
		                  char           const *root_path,
		                  bool                  writable)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.ep().rpc_ep()),
			_label(label),
			_process_packet_dispatcher(env.ep(), *this, &Session_component::_process_packets),
			_vfs(vfs),
			_root(vfs, root_path, false),
			_writable(writable)
		{
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);

			/*
			 * the '/' node is not dynamically allocated, so it is
			 * permanently bound to Dir_handle(0);
			 */
			_nodes[0] = &_root;
			for (unsigned i = 1; i < MAX_NODE_HANDLES; ++i)
				_nodes[i] = nullptr;

			_ram.ref_account(Genode::env()->ram_session_cap());
			Genode::env()->ram_session()->transfer_quota(_ram.cap(), ram_quota);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Genode::Ram_dataspace>(ds));
		}

		void upgrade(char const *args)
		{
			size_t new_quota =
				Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0);
			Genode::env()->ram_session()->transfer_quota(_ram.cap(), new_quota);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(File_system::Path const &path, bool create) override
		{
			if (create && (!_writable))
				throw Permission_denied();

			char const *path_str = path.string();
			/* '/' is bound to '0' */
			if (!strcmp(path_str, "/")) {
				if (create) throw Node_already_exists();
				return Dir_handle(0);
			}

			_assert_valid_path(path_str);
			Vfs_server::Path fullpath(_root.path());
			fullpath.append(path_str);
			path_str = fullpath.base();

			/* make sure a handle is free before allocating */
			auto slot = _next_slot();

			if (!create && !_vfs.directory(path_str))
				throw Lookup_failed();

			Directory *dir;
			try { dir = new (_alloc) Directory(_vfs, path_str, create); }
			catch (Out_of_memory) { throw Out_of_metadata(); }

			_nodes[slot] = dir;
			return Dir_handle(slot);
		}

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode fs_mode, bool create) override
		{
			if ((create || (fs_mode & WRITE_ONLY)) && (!_writable))
				throw Permission_denied();

			Directory &dir = _lookup(dir_handle);

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			/* make sure a handle is free before allocating */
			auto slot = _next_slot();

			File *file = dir.file(_vfs, _alloc, name_str, fs_mode, create);

			_nodes[slot] = file;
			return File_handle(slot);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create) override
		{
			if (create && !_writable) throw Permission_denied();

			Directory &dir = _lookup(dir_handle);

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			/* make sure a handle is free before allocating */
			auto slot = _next_slot();

			Symlink *link = dir.symlink(_vfs, _alloc, name_str,
				_writable ? READ_WRITE : READ_ONLY, create);

			_nodes[slot] = link;
			return Symlink_handle(slot);
		}

		Node_handle node(File_system::Path const &path) override
		{
			char const *path_str = path.string();
			/* '/' is bound to '0' */
			if (!strcmp(path_str, "/"))
				return Node_handle(0);

			_assert_valid_path(path_str);

			/* re-root the path */
			Path sub_path(path_str+1, _root.path());
			path_str = sub_path.base();
			if (!_vfs.leaf_path(path_str))
				throw Lookup_failed();

			auto slot = _next_slot();
			Node *node;

			try { node  = new (_alloc) Node(path_str, STAT_ONLY); }
			catch (Out_of_memory) { throw Out_of_metadata(); }

			_nodes[slot] = node;
			return Node_handle(slot);
		}

		void close(Node_handle handle) override
		{
			/* handle '0' cannot be freed */
			if (!handle.value) {
				_root.notify_listeners();
				return;
			}

			if (!_in_range(handle.value))
				return;

			Node *node = _nodes[handle.value];
			if (!node) { return; }

			node->notify_listeners();

			/*
			 * De-allocate handle
			 */
			Listener &listener = _listeners[handle.value];

			if (listener.valid())
				node->remove_listener(&listener);

			if (File *file = dynamic_cast<File*>(node))
				destroy(_alloc, file);
			else if (Directory *dir = dynamic_cast<Directory*>(node))
				destroy(_alloc, dir);
			else if (Symlink *link = dynamic_cast<Symlink*>(node))
				destroy(_alloc, link);
			else
				destroy(_alloc, node);

			_nodes[handle.value] = 0;
			listener = Listener();
		}

		Status status(Node_handle node_handle) override
		{
			Directory_service::Stat vfs_stat;
			File_system::Status      fs_stat;

			Node &node = _lookup(node_handle);

			if (_vfs.stat(node.path(), vfs_stat) != Directory_service::STAT_OK)
				return fs_stat;

			fs_stat.inode = vfs_stat.inode;

			switch (vfs_stat.mode & (
				Directory_service::STAT_MODE_DIRECTORY |
				Directory_service::STAT_MODE_SYMLINK |
				File_system::Status::MODE_FILE)) {

			case Directory_service::STAT_MODE_DIRECTORY:
				fs_stat.mode = File_system::Status::MODE_DIRECTORY;
				fs_stat.size = _vfs.num_dirent(node.path()) * sizeof(Directory_entry);
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

		void unlink(Dir_handle dir_handle, Name const &name) override
		{
			if (!_writable) throw Permission_denied();

			Directory &dir = _lookup(dir_handle);

			char const *name_str = name.string();
			_assert_valid_name(name_str);

			Path path(name_str, dir.path());

			assert_unlink(_vfs.unlink(path.base()));
			dir.mark_as_updated();
		}

		void truncate(File_handle file_handle, file_size_t size) override {
			_lookup(file_handle).truncate(size); }

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle to_dir_handle,   Name const &to_name) override
		{
			if (!_writable)
				throw Permission_denied();

			char const *from_str = from_name.string();
			char const   *to_str =   to_name.string();

			_assert_valid_name(from_str);
			_assert_valid_name(  to_str);

			Directory &from_dir = _lookup(from_dir_handle);
			Directory   &to_dir = _lookup(  to_dir_handle);

			Path from_path(from_str, from_dir.path());
			Path   to_path(  to_str,   to_dir.path());

			assert_rename(_vfs.rename(from_path.base(), to_path.base()));

			from_dir.mark_as_updated();
			to_dir.mark_as_updated();
		}

		void sigh(Node_handle handle, Signal_context_capability sigh) override
		{
			if (!_in_range(handle.value))
				throw Invalid_handle();

			Node *node = dynamic_cast<Node *>(_nodes[handle.value]);
			if (!node)
				throw Invalid_handle();

			Listener &listener = _listeners[handle.value];

			/*
			 * If there was already a handler registered for the node,
			 * remove the old handler.
			 */
			if (listener.valid())
				node->remove_listener(&listener);

			/*
			 * Register new handler
			 */
			listener = Listener(sigh);
			node->add_listener(&listener);
		}

		/**
		 * Sync the VFS and send any pending signals on the node.
		 */
		void sync(Node_handle handle) override
		{
			try {
				Node &node = _lookup(handle);
				_vfs.sync(node.path());
				node.notify_listeners();
			} catch (Invalid_handle) { }
		}

		void control(Node_handle, Control) override { }
};


class Vfs_server::Root :
	public Genode::Root_component<Session_component>
{
	private:

		Genode::Env  &_env;
		Genode::Heap  _heap { &_env.ram(), &_env.rm() };

		Vfs::Dir_file_system _vfs
			{ _env, _heap, vfs_config(), Vfs::global_file_system_factory() };

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Path session_root;
			bool writeable = false;

			Session_label const label = label_from_args(args);

			char tmp[MAX_PATH_LEN];
			try {
				Session_policy policy(label);

				/* Determine the session root directory.
				 * Defaults to '/' if not specified by session
				 * policy or session arguments.
				 */
				try {
					policy.attribute("root").value(tmp, sizeof(tmp));
					session_root.import(tmp, "/");
				} catch (Xml_node::Nonexistent_attribute) { }

				/* Determine if the session is writeable.
				 * Policy overrides arguments, both default to false.
				 */
				if (policy.attribute_value("writeable", false))
					writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			} catch (Session_policy::No_policy_defined) { }

			Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
			if (Genode::strcmp("/", tmp, sizeof(tmp))) {
				session_root.append("/");
				session_root.append(tmp);
			}

			/*
			 * If no policy matches the client gets
			 * read-only access to the root.
			 */

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").aligned_size();
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").aligned_size();

			if (!tx_buf_size)
				throw Root::Invalid_args();

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size =
				max((size_t)4096, sizeof(Session_component)) +
				tx_buf_size;

			if (session_size > ram_quota) {
				error("insufficient 'ram_quota' from '", label, "' "
				      "got ", ram_quota, ", need ", session_size);
				throw Root::Quota_exceeded();
			}
			ram_quota -= session_size;

			/* check if the session root exists */
			if (!((session_root == "/") || _vfs.directory(session_root.base()))) {
				error("session root '", session_root, "' not found for '", label, "'");
				throw Root::Unavailable();
			}

			Session_component *session = new(md_alloc())
				Session_component(_env,
				                  label.string(),
				                  ram_quota,
				                  tx_buf_size,
				                  _vfs,
				                  session_root.base(),
				                  writeable);

			Genode::log("session opened for '", label, "' at '", session_root, "'");
			return session;
		}

		void _upgrade_session(Session_component *session,
		                      char        const *args) override
		{
			session->upgrade(args);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap { &env.ram(), &env.rm() };

	static Vfs_server::Root root { env, sliced_heap };
}
