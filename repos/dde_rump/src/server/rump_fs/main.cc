/**
 * \brief  RUMP file system interface implementation
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2014-01-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <file_system_session/rpc_object.h>
#include <base/attached_rom_dataspace.h>
#include <timer_session/connection.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <base/component.h>
#include <base/heap.h>

#include "undef.h"

#include <rump/env.h>
#include <rump_fs/fs.h>
#include <sys/resource.h>
#include "file_system.h"
#include "directory.h"
#include "open_node.h"

namespace Rump_fs {

	using File_system::Packet_descriptor;
	using File_system::Path;

	struct Main;
	struct Root;
	struct Session_component;
}

class Rump_fs::Session_component : public Session_rpc_object
{
	private:

		typedef File_system::Open_node<Node> Open_node;

		Allocator                   &_md_alloc;
		Directory                   &_root;
		Id_space<File_system::Node>  _open_node_registry;
		bool                         _writable;

		Signal_handler<Session_component> _process_packet_handler;


		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 *
		 * \return true on success, false on failure
		 */
		void _process_packet_op(Packet_descriptor &packet, Open_node &open_node)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				if (content && (packet.length() <= packet.size()))
					res_length = open_node.node().read((char *)content, length, packet.position());
				break;

			case Packet_descriptor::WRITE:
				if (content && (packet.length() <= packet.size()))
					res_length = open_node.node().write((char const *)content, length, packet.position());
				break;

			case Packet_descriptor::CONTENT_CHANGED:
				open_node.register_notify(*tx_sink());
				/* notify_listeners may bounce the packet back*/
				open_node.node().notify_listeners();
				/* otherwise defer acknowledgement of this packet */
				return;

			case Packet_descriptor::READ_READY:
				/* not supported */
				break;

			case Packet_descriptor::SYNC:
				rump_sys_sync();
				break;
			}

			packet.length(res_length);
			packet.succeeded(res_length > 0);
			tx_sink()->acknowledge_packet(packet);
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			auto process_packet_fn = [&] (Open_node &open_node) {
				_process_packet_op(packet, open_node);
			};

			try {
				_open_node_registry.apply<Open_node>(packet.handle(), process_packet_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				Genode::error("Invalid_handle");
				tx_sink()->acknowledge_packet(packet);
			}
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
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.rm(), env.ep().rpc_ep()),
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
			Rump::env().env().ram().free(static_cap_cast<Ram_dataspace>(ds));
			destroy(&_md_alloc, &_root);
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			auto file_fn = [&] (Open_node &open_node) {

				Node &dir = open_node.node();

				if (!_writable)
					if (create || (mode != STAT_ONLY && mode != READ_ONLY))
						throw Permission_denied();

				File *file = dir.file(name.string(), mode, create);

				Open_node *open_file =
					new (_md_alloc) Open_node(*file, _open_node_registry);

				return open_file->id();
			};

			try {
				return File_handle {
					_open_node_registry.apply<Open_node>(dir_handle, file_fn).value
				};
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{
			if (!File_system::supports_symlinks())
				throw Permission_denied();

			if (!valid_name(name.string()))
				throw Invalid_name();

			auto symlink_fn = [&] (Open_node &open_node) {

				Node &dir = open_node.node();

				if (create && !_writable)
					throw Permission_denied();

				Symlink *link = dir.symlink(name.string(), create);

				Open_node *open_symlink =
					new (_md_alloc) Open_node(*link, _open_node_registry);

				return open_symlink->id();
			};

			try {
				return Symlink_handle {
					_open_node_registry.apply<Open_node>(dir_handle, symlink_fn).value
				};
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
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

			Open_node *open_dir =
				new (_md_alloc) Open_node(*dir, _open_node_registry);

			return Dir_handle { open_dir->id().value };
		}

		Node_handle node(Path const &path)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);
			Node *node = _root.node(path_str + 1);

			Open_node *open_node =
				new (_md_alloc) Open_node(*node, _open_node_registry);

			return open_node->id();
		}

		void close(Node_handle handle)
		{
			auto close_fn = [&] (Open_node &open_node) {
				Node &node = open_node.node();
				destroy(_md_alloc, &open_node);
				destroy(_md_alloc, &node);
			};

			try {
				_open_node_registry.apply<Open_node>(handle, close_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		Status status(Node_handle node_handle)
		{
			auto status_fn = [&] (Open_node &open_node) {
				return open_node.node().status();
			};

			try {
				return _open_node_registry.apply<Open_node>(node_handle, status_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void control(Node_handle, Control) override { }

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			if (!_writable)
				throw Permission_denied();

			auto unlink_fn = [&] (Open_node &open_node) {
				Node &dir = open_node.node();
				dir.unlink(name.string());
			};

			try {
				_open_node_registry.apply<Open_node>(dir_handle, unlink_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			if (!_writable)
				throw Permission_denied();

			auto truncate_fn = [&] (Open_node &open_node) {
				open_node.node().truncate(size);
			};

			try {
				_open_node_registry.apply<Open_node>(file_handle, truncate_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle   to_dir_handle, Name const   &to_name)
		{
			if (!_writable)
				throw Permission_denied();

			auto move_fn = [&] (Open_node &open_from_dir_node) {

				auto inner_move_fn = [&] (Open_node &open_to_dir_node) {

					Node &from_dir = open_from_dir_node.node();
					Node &to_dir = open_to_dir_node.node();

					char const *from_str = from_name.string();
					char const   *to_str =   to_name.string();

					if (!(valid_name(from_str) && valid_name(to_str)))
						throw Lookup_failed();

					if (rump_sys_renameat(from_dir.fd(), from_str,
			                        		to_dir.fd(),   to_str) == 0) {
						from_dir.mark_as_updated();
						from_dir.notify_listeners();
						if (&from_dir != &to_dir) {
							to_dir.mark_as_updated();
							to_dir.notify_listeners();
						}

						return;
					}

					switch (errno) {
					case ENOTEMPTY: throw Node_already_exists();
					case ENOENT: throw Lookup_failed();
					}

					Genode::warning("renameat produced unhandled error ", errno, ", ", from_str, " -> ", to_str);
					throw Permission_denied();

				};

				try {
					_open_node_registry.apply<Open_node>(to_dir_handle, inner_move_fn);
				} catch (Id_space<File_system::Node>::Unknown_id const &) {
					throw Invalid_handle();
				}
			};

			try {
				_open_node_registry.apply<Open_node>(from_dir_handle, move_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}
};

class Rump_fs::Root : public Root_component<Session_component>
{
	private:

		Genode::Env &_env;

		int _sessions { 0 };

		Genode::Attached_rom_dataspace _config { _env, "config" };

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
				throw Service_denied();

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
				throw Insufficient_ram_quota();
			}
			ram_quota -= session_size;

			char tmp[MAX_PATH_LEN];
			try {
				Session_policy policy(label, _config.xml());

				/* determine policy root offset */
				try {
					policy.attribute("root").value(tmp, sizeof(tmp));
					session_root.import(tmp, "/mnt");
				} catch (Xml_node::Nonexistent_attribute) { }

				/*
				 * Determine if the session is writeable.
				 * Policy overrides client argument, both default to false.
				 */
				if (policy.attribute_value("writeable", false))
					writeable = Arg_string::find_arg(args, "writeable").bool_value(false);
			}
			catch (Session_policy::No_policy_defined) { throw Service_denied(); }

			/* apply client's root offset */
			Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
			if (Genode::strcmp("/", tmp, sizeof(tmp))) {
				session_root.append("/");
				session_root.append(tmp);
			}
			session_root.remove_trailing('/');

			char const *root_dir = session_root.base();

			try {
				if (++_sessions == 1) { File_system::mount_fs(); }
			} catch (...) {
				Genode::error("could not mount file system");
				throw Service_denied();
			}

			try {
				return new (md_alloc())
					Session_component(_env, tx_buf_size, root_dir, writeable, *md_alloc());

			} catch (Lookup_failed) {
				Genode::error("File system root directory \"", root_dir, "\" does not exist");
				throw Service_denied();
			}
		}

		void _destroy_session(Session_component *session)
		{
			Genode::destroy(md_alloc(), session);

			try {
				if (--_sessions == 0) { File_system::unmount_fs(); }
			} catch (...) { }
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


struct Rump_fs::Main
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

	Main(Genode::Env &env) : env(env)
	{
		Rump::construct_env(env);

		rump_io_backend_init();

		File_system::init();

			/* set all bits but the stickies */
		rump_sys_umask(S_ISUID|S_ISGID|S_ISVTX);

		/* set open file limit to maximum (256) */
		struct rlimit rl { RLIM_INFINITY, RLIM_INFINITY };
		if (rump_sys_setrlimit(RLIMIT_NOFILE, &rl) != 0)
			Genode::error("rump_sys_setrlimit(RLIMIT_NOFILE, ...) failed, errno ", errno);

		env.parent().announce(env.ep().manage(fs_root));
		env.parent().resource_avail_sigh(resource_handler);

		_timer.sigh(sync_handler);
		_timer.trigger_periodic(2*1000*1000);
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics (uses shared objects) */
	env.exec_static_constructors();

	static Rump_fs::Main inst(env);
}
