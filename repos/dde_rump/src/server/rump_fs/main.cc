/**
 * \brief  RUMP file system interface implementation
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2014-01-14
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system/node_handle_registry.h>
#include <file_system_session/rpc_object.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <base/component.h>
#include <base/heap.h>

#include "undef.h"

#include <rump_fs/fs.h>
#include <sys/resource.h>
#include "file_system.h"
#include "directory.h"

namespace File_system {
	struct Main;
	struct Root;
	struct Session_component;
}

class File_system::Session_component : public Session_rpc_object
{
	private:

		Allocator            &_md_alloc;
		Directory            &_root;
		Node_handle_registry  _handle_registry;
		bool                  _writable;

		Signal_handler<Session_component> _process_packet_handler;


		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 *
		 * \return true on success, false on failure
		 */
		void _process_packet_op(Packet_descriptor &packet, Node &node)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const offset  = packet.position();

			if (!content || (packet.length() > packet.size())) {
				packet.succeeded(false);
				return;
			}

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				res_length = node.read((char *)content, length, offset);
				break;

			case Packet_descriptor::WRITE:
				res_length = node.write((char const *)content, length, offset);
				break;
			}

			packet.length(res_length);
			packet.succeeded(res_length > 0);
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			try {
				Node *node = _handle_registry.lookup(packet.handle());

				_process_packet_op(packet, *node);
			}
			catch (Invalid_handle) { Genode::error("Invalid_handle"); }

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
		static void _assert_valid_path(char const *path)
		{
			if (!path || path[0] != '/')
				throw Lookup_failed();
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env        &env,
		                  size_t              tx_buf_size,
		                  char const         *root_dir,
		                  bool                writeable,
		                  Allocator          &md_alloc)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.ep().rpc_ep()),
			_md_alloc(md_alloc),
			_root(*new (&_md_alloc) Directory(_md_alloc, root_dir, false)),
			_writable(writeable),
			_process_packet_handler(env.ep(), *this, &Session_component::_process_packets)
		{
			/*
			 * Register '_process_packets' dispatch function as signal
			 * handler for packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_handler);
			_tx.sigh_ready_to_ack(_process_packet_handler);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
			destroy(&_md_alloc, &_root);
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			Directory *dir = _handle_registry.lookup(dir_handle);

			if (!_writable)
				if (create || (mode != STAT_ONLY && mode != READ_ONLY))
					throw Permission_denied();

			File *file = dir->file(name.string(), mode, create);
			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{
			if (!File_system::supports_symlinks())
				throw Permission_denied();

			if (!valid_name(name.string()))
				throw Invalid_name();

			Directory *dir = _handle_registry.lookup(dir_handle);

			if (create && !_writable)
				throw Permission_denied();

			Symlink *link = dir->symlink(name.string(), create);
			return  _handle_registry.alloc(link);
		}

		Dir_handle dir(Path const &path, bool create)
		{
			char const *path_str = path.string();
			_assert_valid_path(path_str);

			/* skip leading '/' */
			path_str++;

			if (!_writable && create)
				throw Permission_denied();

			if (!path.valid_string())
				throw Name_too_long();

			Directory *dir = _root.subdir(path_str, create);
			return _handle_registry.alloc(dir);
		}

		Node_handle node(Path const &path)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);
			Node *node = _root.node(path_str + 1);
			Node_handle h =  _handle_registry.alloc(node);
			return h;
		}

		void close(Node_handle handle)
		{
			Node *node = 0;
			
			try {
				node = _handle_registry.lookup(handle);
			} catch (Invalid_handle)  { return; }

			_handle_registry.free(handle);
			/* destruct node */
			if (node)
				destroy(&_md_alloc, node);
		}

		Status status(Node_handle node_handle)
		{
			Node *node = _handle_registry.lookup(node_handle);
			Status s;
			s.inode = node->inode();
			s.size  = 0;
			s.mode  = 0;


			File *file = dynamic_cast<File *>(node);
			if (file) {
				s.size = file->length();
				s.mode = File_system::Status::MODE_FILE;
				return s;
			}

			Directory *dir = dynamic_cast<Directory *>(node);
			if (dir) {
				s.size = dir->num_entries()*sizeof(Directory_entry);
				s.mode = File_system::Status::MODE_DIRECTORY;
				return s;
			}

			Symlink *link = dynamic_cast<Symlink *>(node);
			if (link) {
				s.size = link->length();
				s.mode = File_system::Status::MODE_SYMLINK;
				return s;
			}

			return Status();
		}

		void control(Node_handle, Control) override { }

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			if (!_writable)
				throw Permission_denied();

			Directory *dir = _handle_registry.lookup(dir_handle);
			dir->unlink(name.string());
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			if (!_writable)
				throw Permission_denied();

			File *file = _handle_registry.lookup(file_handle);
			file->truncate(size);
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle   to_dir_handle, Name const   &to_name)
		{
			if (!_writable)
				throw Permission_denied();

			char const *from_str = from_name.string();
			char const   *to_str =   to_name.string();

			if (!(valid_name(from_str) && valid_name(to_str)))
				throw Lookup_failed();

			Directory *from_dir = _handle_registry.lookup(from_dir_handle);
			Directory   *to_dir = _handle_registry.lookup(  to_dir_handle);

			if (rump_sys_renameat(from_dir->fd(), from_str,
			                        to_dir->fd(),   to_str) == 0) {
				from_dir->mark_as_updated();
				from_dir->notify_listeners();
				if (!_handle_registry.refer_to_same_node(from_dir_handle,
				                                         to_dir_handle)) {
					to_dir->mark_as_updated();
					to_dir->notify_listeners();
				}

				return;
			}

			switch (errno) {
			case ENOTEMPTY: throw Node_already_exists();
			case ENOENT: throw Lookup_failed();
			}

			Genode::warning("renameat produced unhandled error ", errno, ", ", from_str, " -> ", to_str);
			throw Permission_denied();
		}

		void sigh(Node_handle node_handle, Signal_context_capability sigh) override
		{
			_handle_registry.sigh(node_handle, sigh);
		}

		void sync(Node_handle) override { rump_sys_sync(); }
};

class File_system::Root : public Root_component<Session_component>
{
	private:

		Genode::Env &_env;

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace Genode;

			/*
			 * Determine client-specific policy defined implicitly by
			 * the client's label.
			 */

			Genode::Path<MAX_PATH_LEN> session_root;
			bool writeable = false;

			Session_label const label = label_from_args(args);

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
				Genode::error("insufficient 'ram_quota' from ", label.string(),
				              " got ", ram_quota, "need ", session_size);
				throw Root::Quota_exceeded();
			}
			ram_quota -= session_size;

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

			} catch (...) { }

			/*
			 * If no policy matches the client gets
			 * read-only access to the root.
			 */

			Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
			if (Genode::strcmp("/", tmp, sizeof(tmp))) {
				session_root.append("/");
				session_root.append(tmp);
			}
			session_root.remove_trailing('/');

			char const *root_dir = session_root.base();

			try {
				return new (md_alloc())
					Session_component(_env, tx_buf_size, root_dir, writeable, *md_alloc());
			} catch (Lookup_failed) {
				Genode::error("File system root directory \"", root_dir, "\" does not exist");
				throw Root::Unavailable();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Root(Genode::Env &env, Allocator &md_alloc)
		:
			Root_component<Session_component>(env.ep(), md_alloc),
			_env(env)
		{ }
};


struct File_system::Main
{
	Genode::Env &env;

	Timer::Connection _timer { env };

	/* return immediately from resource requests */
	void ignore_resource() { }

	Genode::Signal_handler<Main> resource_handler
		{ env.ep(), *this, &Main::ignore_resource };

	/* periodic sync */
	void sync()
	{
		/* sync through front-end */
		rump_sys_sync();
		/* sync Genode back-end */
		rump_io_backend_sync();
	}

	Genode::Signal_handler<Main> sync_handler
		{ env.ep(), *this, &Main::sync };

	Heap heap { env.ram(), env.rm() };

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root fs_root { env, sliced_heap };

	Attached_rom_dataspace config { env, "config" };

	Main(Genode::Env &env) : env(env)
	{
		File_system::init(env, heap, config.xml());

			/* set all bits but the stickies */
		rump_sys_umask(S_ISUID|S_ISGID|S_ISVTX);

		/* set open file limit to maximum (256) */
		struct rlimit rl { RLIM_INFINITY, RLIM_INFINITY };
		if (rump_sys_setrlimit(RLIMIT_NOFILE, &rl) != 0)
			Genode::error("rump_sys_setrlimit(RLIMIT_NOFILE, ...) failed, errno ", errno);

		env.parent().announce(env.ep().manage(fs_root));
		env.parent().resource_avail_sigh(resource_handler);

		_timer.sigh(sync_handler);
		_timer.trigger_periodic(10*1000*1000);
	}
};


void Component::construct(Genode::Env &env) { static File_system::Main inst(env); }
