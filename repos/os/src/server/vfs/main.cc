/*
 * \brief  VFS File_system server
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2015-08-16
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/registry.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <os/session_policy.h>
#include <base/allocator_guard.h>
#include <vfs/dir_file_system.h>
#include <vfs/file_system_factory.h>

/* Local includes */
#include "assert.h"
#include "node.h"


namespace Vfs_server {
	using namespace File_system;
	using namespace Vfs;

	class Session_component;
	class Io_response_handler;
	class Watch_response_handler;
	class Vfs_env;
	class Root;

	typedef Genode::Registered<Session_component> Registered_session;
	typedef Genode::Registry<Registered_session>  Session_registry;

	/**
	 * Convenience utities for parsing quotas
	 */
	Genode::Ram_quota parse_ram_quota(char const *args) {
		return Genode::Ram_quota{
			Genode::Arg_string::find_arg(args, "ram_quota").ulong_value(0)}; }
	Genode::Cap_quota parse_cap_quota(char const *args) {
		return Genode::Cap_quota{
			Genode::Arg_string::find_arg(args, "cap_quota").ulong_value(0)}; }
};


class Vfs_server::Session_component : public File_system::Session_rpc_object,
                                      public Session_io_handler
{
	private:

		Node_space _node_space { };

		Genode::Ram_quota_guard           _ram_guard;
		Genode::Cap_quota_guard           _cap_guard;
		Genode::Constrained_ram_allocator _ram_alloc;
		Genode::Heap                      _alloc;

		Genode::Signal_handler<Session_component> _process_packet_handler;

		Vfs::File_system &_vfs;

		/*
		 * The root node needs be allocated with the session struct
		 * but removeable from the id space at session destruction.
		 */
		Path const _root_path;

		Genode::Session_label const _label;

		bool const _writable;

		/*
		 * XXX Currently, we have only one packet in backlog, which must finish
		 *     processing before new packets can be processed.
		 */
		Packet_descriptor _backlog_packet { };

		/****************************
		 ** Handle to node mapping **
		 ****************************/

		/**
		 * Apply functor to node
		 *
		 * \throw Invalid_handle
		 */
		template <typename FUNC>
		void _apply_node(Node_handle handle, FUNC const &fn)
		{
			Node_space::Id id { handle.value };

			try { return _node_space.apply<Node>(id, fn); }
			catch (Node_space::Unknown_id) { throw Invalid_handle(); }
		}

		/**
		 * Apply functor to typed node
		 *
		 * \throw Invalid_handle
		 */
		template <typename HANDLE_TYPE, typename FUNC>
		auto _apply(HANDLE_TYPE handle, FUNC const &fn)
		-> typename Genode::Trait::Functor<decltype(&FUNC::operator())>::Return_type
		{
			Node_space::Id id { handle.value };

			try { return _node_space.apply<Node>(id, [&] (Node &node) {
				typedef typename Node_type<HANDLE_TYPE>::Type Typed_node;
				Typed_node *n = dynamic_cast<Typed_node *>(&node);
				if (!n)
					throw Invalid_handle();
				return fn(*n);
			}); } catch (Node_space::Unknown_id) { throw Invalid_handle(); }
		}


		/******************************
		 ** Packet-stream processing **
		 ******************************/

		struct Not_ready { };
		struct Dont_ack { };

		/**
		 * Perform packet operation
		 *
		 * \throw Not_ready
		 * \throw Dont_ack
		 */
		void _process_packet_op(Packet_descriptor &packet)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const seek    = packet.position();

			/* assume failure by default */
			packet.succeeded(false);

			if ((packet.length() > packet.size()))
				return;

			/* resulting length */
			size_t res_length = 0;
			bool succeeded = false;

			switch (packet.operation()) {

			case Packet_descriptor::READ:

				try {
					_apply(packet.handle(), [&] (Io_node &node) {
						if (!node.read_ready()) {
							node.notify_read_ready(true);
							throw Not_ready();
						}

						if (node.mode() & READ_ONLY) {
							res_length = node.read((char *)content, length, seek);
							/* no way to distinguish EOF from unsuccessful
							   reads, both have res_length == 0 */
							succeeded = true;
						}
					});
				}
				catch (Not_ready) { throw; }
				catch (Operation_incomplete) { throw Not_ready(); }
				catch (...) { }

				break;

			case Packet_descriptor::WRITE:

				try {
					_apply(packet.handle(), [&] (Io_node &node) {
						if (node.mode() & WRITE_ONLY) {
							res_length = node.write((char const *)content, length, seek);

							/* File system session can't handle partial writes */
							if (res_length != length) {
								Genode::error("partial write detected ",
								              res_length, " vs ", length);
								/* don't acknowledge */
								throw Dont_ack();
							}
							succeeded = true;
						}
					});
				} catch (Operation_incomplete) {
					throw Not_ready();
				} catch (...) { }
				break;

			case Packet_descriptor::READ_READY:

				try {
					_apply(static_cast<File_handle>(packet.handle().value), [] (File &node) {
						if (!node.read_ready()) {
							node.notify_read_ready(true);
							throw Dont_ack();
						}
					});
					succeeded = true;
				}
				catch (Dont_ack) { throw; }
				catch (...) { }
				break;

			case Packet_descriptor::CONTENT_CHANGED:
				Genode::warning("ignoring CONTENT_CHANGED packet from client");
				throw Dont_ack();

			case Packet_descriptor::SYNC:

				/**
				 * Sync the VFS and send any pending signals on the node.
				 */
				try {
					_apply(packet.handle(), [&] (Io_node &node) {
						succeeded = node.sync();
					});
				} catch (Operation_incomplete) {
					throw Not_ready();
				} catch (...) { Genode::error("SYNC: unhandled exception"); }
				break;
			}

			packet.length(res_length);
			packet.succeeded(succeeded);
		}

		bool _try_process_packet_op(Packet_descriptor &packet)
		{
			try {
				_process_packet_op(packet);
				return true;
			} catch (Not_ready) {
				_backlog_packet = packet;
			}

			return false;
		}

		bool _process_backlog()
		{
			/* indicate success if there's no backlog */
			if (!_backlog_packet.size() &&
			    (_backlog_packet.operation() != Packet_descriptor::SYNC)) {
				return true;
			}

			/* only start processing if acknowledgement is possible */
			if (!tx_sink()->ready_to_ack())
				return false;

			if (!_try_process_packet_op(_backlog_packet))
				return false;

			/*
			 * The 'acknowledge_packet' function cannot block because we
			 * checked for 'ready_to_ack' in '_process_packets'.
			 */
			tx_sink()->acknowledge_packet(_backlog_packet);

			/* invalidate backlog packet */
			_backlog_packet = Packet_descriptor();

			return true;
		}

		bool _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			if (!_try_process_packet_op(packet))
				return false;

			/*
			 * The 'acknowledge_packet' function cannot block because we
			 * checked for 'ready_to_ack' in '_process_packets'.
			 */
			tx_sink()->acknowledge_packet(packet);

			return true;
		}

		/**
		 * Called by signal dispatcher, executed in the context of the main
		 * thread (not serialized with the RPC functions)
		 */
		void _process_packets()
		{
			using namespace Genode;

			/*
			 * XXX Process client backlog before looking at new requests. This
			 *     limits the number of simultaneously addressed handles (which
			 *     was also the case before adding the backlog in case of
			 *     blocking operations).
			 */
			if (!_process_backlog())
				/* backlog not cleared - block for next condition change */
				return;

			/**
			 * Process packets in batches, otherwise a client that
			 * submits packets as fast as they are processed will
			 * starve the signal handler.
			 */
			int quantum = TX_QUEUE_SIZE;

			while (tx_sink()->packet_avail()) {
				if (--quantum == 0) {
					/* come back to this later */
					Signal_transmitter(_process_packet_handler).submit();
					break;
				}

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

				try {
					if (!_process_packet())
						return;
				} catch (Dont_ack) { }
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

		void _close(Node &node)
		{
			if (File *file = dynamic_cast<File*>(&node))
				destroy(_alloc, file);
			else if (Directory *dir = dynamic_cast<Directory*>(&node))
				destroy(_alloc, dir);
			else if (Symlink *link = dynamic_cast<Symlink*>(&node))
				destroy(_alloc, link);
			else if (Watch_node *watch = dynamic_cast<Watch_node*>(&node))
				destroy(_alloc, watch);
			else
				destroy(_alloc, &node);
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

		Session_component(Genode::Env          &env,
		                  char           const *label,
		                  Genode::Ram_quota     ram_quota,
		                  Genode::Cap_quota     cap_quota,
		                  size_t                tx_buf_size,
		                  Vfs::File_system     &vfs,
		                  char           const *root_path,
		                  bool                  writable)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.rm(), env.ep().rpc_ep()),
			_ram_guard(ram_quota),
			_cap_guard(cap_quota),
			_ram_alloc(env.pd(), _ram_guard, _cap_guard),
			_alloc(_ram_alloc, env.rm()),
			_process_packet_handler(env.ep(), *this, &Session_component::_process_packets),
			_vfs(vfs),
			_root_path(root_path),
			_label(label),
			_writable(writable)
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
			while (_node_space.apply_any<Node>([&] (Node &node) {
				_close(node); })) { }
		}

		/**
		 * Increase quotas
		 */
		void upgrade(Genode::Ram_quota ram) {
			_ram_guard.upgrade(ram); }
		void upgrade(Genode::Cap_quota caps) {
			_cap_guard.upgrade(caps); }

		/*
		 * Called by the IO response handler for events which are not
		 * node-specific, for example after 'release_packet()' to signal
		 * that a previously failed 'alloc_packet()' may succeed now.
		 */
		void handle_general_io() {
			_process_packets(); }


		/********************************
		 **  Node_io_handler interface **
		 ********************************/

		void handle_node_io(Io_node &node) override
		{
			_process_backlog();

			if (!tx_sink()->ready_to_ack()) {
				Genode::error(
					"dropping I/O notfication, congested packet buffer to '", _label, "'");
			}

			if (node.notify_read_ready() && node.read_ready()
			 && tx_sink()->ready_to_ack()) {
				Packet_descriptor packet(Packet_descriptor(),
				                         Node_handle { node.id().value },
				                         Packet_descriptor::READ_READY,
				                         0, 0);
				packet.succeeded(true);
				tx_sink()->acknowledge_packet(packet);
				node.notify_read_ready(false);
			}
		}

		void handle_node_watch(Watch_node &node) override
		{
			if (!tx_sink()->ready_to_ack()) {
				Genode::error(
					"dropping watch notfication, congested packet buffer to '", _label, "'");
			} else {
				Packet_descriptor packet(Packet_descriptor(),
				                         Node_handle { node.id().value },
				                         Packet_descriptor::CONTENT_CHANGED,
				                         0, 0);
				tx_sink()->acknowledge_packet(packet);
			}
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(File_system::Path const &path, bool create) override
		{
			if (create && (!_writable))
				throw Permission_denied();

			char const *path_str = path.string();

			if (!strcmp(path_str, "/") && create)
				throw Node_already_exists();

			_assert_valid_path(path_str);
			Vfs_server::Path fullpath(_root_path);
			if (path_str[1] != '\0')
				fullpath.append(path_str);
			path_str = fullpath.base();

			if (!create && !_vfs.directory(path_str))
				throw Lookup_failed();

			Directory *dir;
			try { dir = new (_alloc) Directory(_node_space, _vfs, _alloc,
			                                  *this, path_str, create); }
			catch (Out_of_memory) { throw Out_of_ram(); }

			return Dir_handle(dir->id().value);
		}

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode fs_mode, bool create) override
		{
			if ((create || (fs_mode & WRITE_ONLY)) && (!_writable))
				throw Permission_denied();

			return _apply(dir_handle, [&] (Directory &dir) {
				char const *name_str = name.string();
				_assert_valid_name(name_str);

				return File_handle {
					dir.file(_node_space, _vfs, _alloc, name_str, fs_mode, create).value
				};
			});
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create) override
		{
			if (create && !_writable) throw Permission_denied();

			return _apply(dir_handle, [&] (Directory &dir) {
				char const *name_str = name.string();
				_assert_valid_name(name_str);

				return Symlink_handle {dir.symlink(
					_node_space, _vfs, _alloc, name_str,
					_writable ? READ_WRITE : READ_ONLY, create).value
				};
			});
		}

		Node_handle node(File_system::Path const &path) override
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			/* re-root the path */
			Path sub_path(path_str+1, _root_path.base());
			path_str = sub_path.base();
			if (!_vfs.leaf_path(path_str))
				throw Lookup_failed();

			Node *node;

			try { node  = new (_alloc) Node(_node_space, path_str, *this); }
			catch (Out_of_memory) { throw Out_of_ram(); }

			return Node_handle { node->id().value };
		}

		Watch_handle watch(File_system::Path const &path) override
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			/* re-root the path */
			Path sub_path(path_str+1, _root_path.base());
			path_str = sub_path.base();

			Vfs::Vfs_watch_handle *vfs_handle = nullptr;
			typedef Directory_service::Watch_result Result;
			switch (_vfs.watch(path_str, &vfs_handle, _alloc)) {
			case Result::WATCH_OK: break;
			case Result::WATCH_ERR_UNACCESSIBLE:
				throw Lookup_failed();
			case Result::WATCH_ERR_STATIC:
				throw Unavailable();
			case Result::WATCH_ERR_OUT_OF_RAM:
				throw Out_of_ram();
			case Result::WATCH_ERR_OUT_OF_CAPS:
				throw Out_of_caps();
			}

			Node *node;
			try { node  = new (_alloc)
				Watch_node(_node_space, path_str, *vfs_handle, *this); }
			catch (Out_of_memory) { throw Out_of_ram(); }

			return Watch_handle { node->id().value };
		}

		void close(Node_handle handle) override
		{
			try { _apply_node(handle, [&] (Node &node) {
				_close(node);
			}); } catch (File_system::Invalid_handle) { }
		}

		Status status(Node_handle node_handle) override
		{
			File_system::Status      fs_stat;

			_apply_node(node_handle, [&] (Node &node) {
				Directory_service::Stat vfs_stat;

				if (_vfs.stat(node.path(), vfs_stat) != Directory_service::STAT_OK)
					throw Invalid_handle();

				fs_stat.inode = vfs_stat.inode;

				switch (vfs_stat.mode & (
					Directory_service::STAT_MODE_DIRECTORY |
					Directory_service::STAT_MODE_SYMLINK |
					File_system::Status::MODE_FILE)) {

				case Directory_service::STAT_MODE_DIRECTORY:
					fs_stat.mode = File_system::Status::MODE_DIRECTORY;
					fs_stat.size = _vfs.num_dirent(node.path()) * sizeof(Directory_entry);
					return;

				case Directory_service::STAT_MODE_SYMLINK:
					fs_stat.mode = File_system::Status::MODE_SYMLINK;
					break;

				default: /* Directory_service::STAT_MODE_FILE */
					fs_stat.mode = File_system::Status::MODE_FILE;
					break;
				}

				fs_stat.size = vfs_stat.size;
			});
			return fs_stat;
		}

		void unlink(Dir_handle dir_handle, Name const &name) override
		{
			if (!_writable) throw Permission_denied();

			_apply(dir_handle, [&] (Directory &dir) {
				char const *name_str = name.string();
				_assert_valid_name(name_str);

				Path path(name_str, dir.path());

				assert_unlink(_vfs.unlink(path.base()));
				dir.mark_as_updated();
			});
		}

		void truncate(File_handle file_handle, file_size_t size) override
		{
			_apply(file_handle, [&] (File &file) {
				file.truncate(size); });
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
		          Dir_handle to_dir_handle,   Name const &to_name) override
		{
			if (!_writable)
				throw Permission_denied();

			char const *from_str = from_name.string();
			char const   *to_str =   to_name.string();

			_assert_valid_name(from_str);
			_assert_valid_name(  to_str);

			_apply(from_dir_handle, [&] (Directory &from_dir) {
				_apply(to_dir_handle, [&] (Directory &to_dir) {
					Path from_path(from_str, from_dir.path());
					Path   to_path(  to_str,   to_dir.path());

					assert_rename(_vfs.rename(from_path.base(), to_path.base()));

					from_dir.mark_as_updated();
					to_dir.mark_as_updated();
				});
			});
		}

		void control(Node_handle, Control) override { }
};

/**
 * Global I/O event handler
 */
struct Vfs_server::Io_response_handler : Vfs::Io_response_handler
{
	Session_registry &_session_registry;

	bool _in_progress  { false };
	bool _handle_general_io { false };

	Io_response_handler(Session_registry &session_registry)
	: _session_registry(session_registry) { }

	void handle_io_response(Vfs::Vfs_handle::Context *context) override
	{
		if (_in_progress) {
			/* called recursively, context is nullptr in this case */
			_handle_general_io = true;
			return;
		}

		_in_progress = true;

		if (context)
			Io_node::node_by_context(*context).handle_io_response();
		else
			_handle_general_io = true;

		while (_handle_general_io) {
			_handle_general_io = false;
			_session_registry.for_each([ ] (Registered_session &r) {
				r.handle_general_io();
			});
		}

		_in_progress = false;
	}
};

/**
 * Global VFS watch handler
 */
struct Vfs_server::Watch_response_handler : Vfs::Watch_response_handler
{
	void handle_watch_response(Vfs::Vfs_watch_handle::Context *context) override
	{
		if (context)
			Watch_node::node_by_context(*context).handle_watch_response();
	}
};


class Vfs_server::Vfs_env final : Vfs::Env
{
	private:

		Genode::Env  &_env;
		Genode::Heap  _heap { &_env.ram(), &_env.rm() };

		Io_response_handler    _io_handler;
		Watch_response_handler _watch_handler { };

		Vfs::Global_file_system_factory _global_file_system_factory { _heap };

		Vfs::Dir_file_system _root_dir;

	public:

		Vfs_env(Genode::Env &env, Genode::Xml_node config,
		        Session_registry &sessions)
		: _env(env), _io_handler(sessions),
		  _root_dir(*this, config, _global_file_system_factory)
		{ }

		Genode::Env &env() override { return _env; }

		Genode::Allocator &alloc() override { return _heap; }

		Vfs::File_system &root_dir() override { return _root_dir; }

		Io_response_handler &io_handler() override {
			return _io_handler; }

		Watch_response_handler &watch_handler() override {
			return _watch_handler; }
};


class Vfs_server::Root : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _config_rom { _env, "config" };

		Genode::Xml_node vfs_config()
		{
			try { return _config_rom.xml().sub_node("vfs"); }
			catch (...) {
				Genode::error("VFS not configured");
				_env.parent().exit(~0);
				throw;
			}
		}

		Session_registry _session_registry { };

		Vfs_env _vfs_env { _env, vfs_config(), _session_registry };

		Genode::Signal_handler<Root> _config_handler {
			_env.ep(), *this, &Root::_config_update };

		void _config_update()
		{
			_config_rom.update();
			_vfs_env.root_dir().apply_config(vfs_config());
		}

	protected:

		Session_component *_create_session(const char *args) override
		{
			using namespace Genode;

			Session_label const label = label_from_args(args);
			Path session_root;
			bool writeable = false;

			/*****************
			 ** Quota check **
			 *****************/

			auto const initial_ram_usage = _env.pd().used_ram().value;
			auto const initial_cap_usage = _env.pd().used_caps().value;

			auto const ram_quota = parse_ram_quota(args).value;
			auto const cap_quota = parse_cap_quota(args).value;

			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").aligned_size();

			if (!tx_buf_size)
				throw Service_denied();

			size_t session_size =
				max((size_t)4096, sizeof(Session_component)) +
				tx_buf_size;

			if (session_size > ram_quota) {
				error("insufficient 'ram_quota' from '", label, "' "
				      "got ", ram_quota, ", need ", session_size);
				throw Insufficient_ram_quota();
			}


			/**************************
			 ** Apply session policy **
			 **************************/

			/* pull in policy changes */
			_config_rom.update();

			char tmp[MAX_PATH_LEN];
			try {
				Session_policy policy(label, _config_rom.xml());

				/* determine optional session root offset. */
				try {
					policy.attribute("root").value(tmp, sizeof(tmp));
					session_root.import(tmp, "/");
				} catch (Xml_node::Nonexistent_attribute) { }

				/*
				 * Determine if the session is writeable.
				 * Policy overrides client argument, both default to false.
				 */
				if (policy.attribute_value("writeable", false))
					writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			} catch (Session_policy::No_policy_defined) {
				/* missing policy - deny request */
				throw Service_denied();
			}

			/* apply client's root offset. */
			Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
			if (Genode::strcmp("/", tmp, sizeof(tmp))) {
				session_root.append("/");
				session_root.append(tmp);
				session_root.remove_trailing('/');
			}

			/* check if the session root exists */
			if (!((session_root == "/")
			 || _vfs_env.root_dir().directory(session_root.base()))) {
				error("session root '", session_root, "' not found for '", label, "'");
				throw Service_denied();
			}

			Session_component *session = new (md_alloc())
				Registered_session(_session_registry, _env, label.string(),
				                   Genode::Ram_quota{ram_quota},
				                   Genode::Cap_quota{cap_quota},
				                   tx_buf_size, _vfs_env.root_dir(),
				                   session_root.base(), writeable);

			auto ram_used = _env.pd().used_ram().value - initial_ram_usage;
			auto cap_used = _env.pd().used_caps().value - initial_cap_usage;

			if ((ram_used > ram_quota) || (cap_used > cap_quota)) {
				if (ram_used > ram_quota)
					Genode::warning("ram donation is ", ram_quota,
					                " but used RAM is ", ram_used, "B"
					                ", '", label, "'");
				if (cap_used > cap_quota)
					Genode::warning("cap donation is ", cap_quota,
					                " but used caps is ", cap_used,
					                ", '", label, "'");
			}

			Genode::log("session opened for '", label, "' at '", session_root, "'");
			return session;
		}

		void _upgrade_session(Session_component *session,
		                      char        const *args) override
		{
			Genode::Ram_quota more_ram = parse_ram_quota(args);
			Genode::Cap_quota more_caps = parse_cap_quota(args);

			if (more_ram.value > 0)
				session->upgrade(more_ram);
			if (more_caps.value > 0)
				session->upgrade(more_caps);
		}

	public:

		Root(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			_config_rom.sigh(_config_handler);
			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap { &env.ram(), &env.rm() };

	static Vfs_server::Root root { env, sliced_heap };
}
