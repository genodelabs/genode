/*
 * \brief  VFS File_system server
 * \author Emery Hemingway
 * \author Christian Helmuth
 * \date   2015-08-16
 */

/*
 * Copyright (C) 2015-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/registry.h>
#include <base/heap.h>
#include <base/attached_ram_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <os/session_policy.h>
#include <base/allocator_guard.h>
#include <util/fifo.h>
#include <vfs/simple_env.h>

/* Local includes */
#include "assert.h"
#include "node.h"


namespace Vfs_server {
	using namespace File_system;
	using namespace Vfs;

	class Session_resources;
	class Session_component;
	class Vfs_env;
	class Root;

	typedef Genode::Fifo<Session_component> Session_queue;

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


/**
 * Base class to manage session quotas and allocations
 */
class Vfs_server::Session_resources
{
	protected:

		Genode::Ram_quota_guard           _ram_guard;
		Genode::Cap_quota_guard           _cap_guard;
		Genode::Constrained_ram_allocator _ram_alloc;
		Genode::Attached_ram_dataspace    _packet_ds;
		Genode::Heap                      _alloc;

		Session_resources(Genode::Ram_allocator &ram,
		                  Genode::Region_map    &region_map,
		                  Genode::Ram_quota     ram_quota,
		                  Genode::Cap_quota     cap_quota,
		                  Genode::size_t        buffer_size)
		:
			_ram_guard(ram_quota), _cap_guard(cap_quota),
			_ram_alloc(ram, _ram_guard, _cap_guard),
			_packet_ds(_ram_alloc, region_map, buffer_size),
			_alloc(_ram_alloc, region_map)
		{ }
};


class Vfs_server::Session_component : private Session_resources,
                                      public ::File_system::Session_rpc_object,
                                      private Session_queue::Element
{
	friend Session_queue;

	private:

		Vfs::File_system &_vfs;

		Genode::Entrypoint &_ep;

		Packet_stream &_stream { *tx_sink() };

		/* global queue of nodes to process after an I/O signal */
		Node_queue &_pending_nodes;

		/* global queue of sessions for which packets await progress */
		Session_queue &_pending_sessions;

		/* collection of open nodes local to this session */
		Node_space _node_space { };

		Genode::Signal_handler<Session_component> _process_packet_handler {
			_ep, *this, &Session_component::_process_packets };

		/*
		 * The root node needs be allocated with the session struct
		 * but removeable from the id space at session destruction.
		 */
		Path const _root_path;

		Genode::Session_label const _label;

		bool const _writable;


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

		/**
		 * Attempt to process the head of the packet queue
		 *
		 * Return true if the packet can be popped from the
		 * queue or false if the the packet cannot be processed
		 * or further queued.
		 */
		bool _process_packet()
		{
			/* leave the packet queued so that it cannot leak */
			Packet_descriptor packet = _stream.peek_packet();

			/* assume failure by default */
			packet.succeeded(false);

			if ((packet.length() > packet.size())) {
				/* not a valid packet */
				_stream.acknowledge_packet(packet);
				return true;
			}

			bool handle_invalid = true;
			bool result = true;

			try {
				_apply(packet.handle(), [&] (Io_node &node) {
					handle_invalid = false;
					result = node.process_packet(packet);
				});
			}
			catch (File_system::Invalid_handle) { }

			/* send the packet back if the handle is missing */
			if (handle_invalid)
				_stream.acknowledge_packet(packet);

			return (handle_invalid || result);
		}

	protected:

		friend Vfs_server::Root;
		using Session_queue::Element::enqueued;

		/**
		 * Called by the global Io_progress_handler as
		 * well as the local signal handler
		 *
		 * Return true if the packet queue was emptied
		 */
		bool process_packets()
		{
			/**
			 * Process packets in batches, otherwise a client that
			 * submits packets as fast as they are processed will
			 * starve the signal handler.
			 */
			int quantum = TX_QUEUE_SIZE;

			while (_stream.packet_avail()) {
				if (_process_packet()) {
					/*
					 * the packet was rejected or associated with
					 * a handle, pop it from the packet queue
					 */
					_stream.get_packet();
				} else {
					/* no progress */
					return false;
				}

				if (--quantum == 0) {
					/* come back to this later */
					Genode::Signal_transmitter(_process_packet_handler).submit();
					return false;
				}
			}

			return true;
		}

	private:

		/**
		 * Called by signal handler
		 */
		void _process_packets()
		{
			bool done = process_packets();

			if (done && enqueued()) {
				/* this session is idle */
				_pending_sessions.remove(*this);
			} else
			if (!done && !enqueued()) {
				/* this session needs unblocking */
				_pending_sessions.enqueue(*this);
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

		/**
		 * Destroy an open node
		 */
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
		 */
		Session_component(Genode::Env          &env,
		                  char           const *label,
		                  Genode::Ram_quota     ram_quota,
		                  Genode::Cap_quota     cap_quota,
		                  size_t                tx_buf_size,
		                  Vfs::File_system     &vfs,
		                  Node_queue           &pending_nodes,
		                  Session_queue        &pending_sessions,
		                  char           const *root_path,
		                  bool                  writable)
		:
			Session_resources(env.pd(), env.rm(), ram_quota, cap_quota, tx_buf_size),
			Session_rpc_object(_packet_ds.cap(), env.rm(), env.ep().rpc_ep()),
			_vfs(vfs),
			_ep(env.ep()),
			_pending_nodes(pending_nodes),
			_pending_sessions(pending_sessions),
			_root_path(root_path),
			_label(label),
			_writable(writable)
		{
			/*
			 * Register an I/O signal handler for
			 * packet-avail and ready-to-ack signals.
			 */
			_tx.sigh_packet_avail(_process_packet_handler);
			_tx.sigh_ready_to_ack(_process_packet_handler);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			/* flush and close the open handles */
			while (_node_space.apply_any<Node>([&] (Node &node) {
				_close(node); })) { }

			if (enqueued())
				_pending_sessions.remove(*this);
		}

		/**
		 * Increase quotas
		 */
		void upgrade(Genode::Ram_quota ram) {
			_ram_guard.upgrade(ram); }
		void upgrade(Genode::Cap_quota caps) {
			_cap_guard.upgrade(caps); }


		/***************************
		 ** File_system interface **
		 ***************************/

		Dir_handle dir(::File_system::Path const &path, bool create) override
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
			                                   _pending_nodes, _stream,
			                                   path_str, create); }
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

		Node_handle node(::File_system::Path const &path) override
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			/* re-root the path */
			Path sub_path(path_str+1, _root_path.base());
			path_str = sub_path.base();
			if (sub_path != "/" && !_vfs.leaf_path(path_str))
				throw Lookup_failed();

			Node *node;

			try { node  = new (_alloc) Node(_node_space, path_str,
			                                _pending_nodes, _stream); }
			catch (Out_of_memory) { throw Out_of_ram(); }

			return Node_handle { node->id().value };
		}

		Watch_handle watch(::File_system::Path const &path) override
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
				Watch_node(_node_space, path_str, *vfs_handle,
				           _pending_nodes, _stream); }
			catch (Out_of_memory) { throw Out_of_ram(); }

			return Watch_handle { node->id().value };
		}

		void close(Node_handle handle) override
		{
			/*
			 * churn the packet queue so that any pending
			 * packets on this handle are processed
			 */
			process_packets();

			try { _apply_node(handle, [&] (Node &node) {
				_close(node); }); }
			catch (::File_system::Invalid_handle) { }
		}

		Status status(Node_handle node_handle) override
		{
			::File_system::Status fs_stat;

			_apply_node(node_handle, [&] (Node &node) {
				Directory_service::Stat vfs_stat;

				if (_vfs.stat(node.path(), vfs_stat) != Directory_service::STAT_OK)
					throw Invalid_handle();

				fs_stat.inode = vfs_stat.inode;

				switch (vfs_stat.mode & (
					Directory_service::STAT_MODE_DIRECTORY |
					Directory_service::STAT_MODE_SYMLINK |
					::File_system::Status::MODE_FILE)) {

				case Directory_service::STAT_MODE_DIRECTORY:
					fs_stat.mode = ::File_system::Status::MODE_DIRECTORY;
					fs_stat.size = _vfs.num_dirent(node.path()) * sizeof(Directory_entry);
					return;

				case Directory_service::STAT_MODE_SYMLINK:
					fs_stat.mode = ::File_system::Status::MODE_SYMLINK;
					break;

				default: /* Directory_service::STAT_MODE_FILE */
					fs_stat.mode = ::File_system::Status::MODE_FILE;
					break;
				}

				fs_stat.size = vfs_stat.size;
				fs_stat.modification_time.value = vfs_stat.modification_time.value;
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

		Genode::Signal_handler<Root> _config_handler {
			_env.ep(), *this, &Root::_config_update };

		void _config_update()
		{
			_config_rom.update();
			_vfs_env.root_dir().apply_config(vfs_config());
		}

		/**
		 * The VFS uses an internal heap that
		 * subtracts from the component quota
		 */
		Genode::Heap    _vfs_heap { &_env.ram(), &_env.rm() };
		Vfs::Simple_env _vfs_env  { _env, _vfs_heap, vfs_config() };

		/**
		 * Object for post-I/O-signal processing
		 *
		 * This allows packet and VFS backend signals to
		 * be dispatched quickly followed by a processing
		 * of sessions that might be unblocked.
		 */
		struct Io_progress_handler : Genode::Entrypoint::Io_progress_handler
		{
			/* All nodes with a packet operation awaiting an I/O signal */
			Node_queue pending_nodes { };

			/* All sessions with packet queues that await processing */
			Session_queue pending_sessions { };

			/**
			 * Post-signal hook invoked by entrypoint
			 */
			void handle_io_progress() override
			{
				bool handle_progress = false;

				/* process handles awaiting progress */
				{
					/* nodes to process later */
					Node_queue retry { };

					/* empty the pending nodes and process */
					pending_nodes.dequeue_all([&] (Node &node) {
						if (node.process_io()) {
							handle_progress = true;
						} else {
							if (!node.enqueued()) {
								retry.enqueue(node);
							}
						}
					});

					/* requeue the unprocessed nodes in order */
					retry.dequeue_all([&] (Node &node) {
						pending_nodes.enqueue(node); });
				}

				/*
				 * if any pending handles were processed then
				 * process session packet queues awaiting progress
				 */
				if (handle_progress) {
					/* sessions to process later */
					Session_queue retry { };

					/* empty the pending nodes and process */
					pending_sessions.dequeue_all([&] (Session_component &session) {
						if (!session.process_packets()) {
							/* requeue the session if there are packets remaining */
							if (!session.enqueued()) {
								retry.enqueue(session);
							}
						}
					});

					/* requeue the unprocessed sessions in order */
					retry.dequeue_all([&] (Session_component &session) {
						pending_sessions.enqueue(session); });
				}
			}
		} _progress_handler { };

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

			typedef String<MAX_PATH_LEN> Root_path;

			Session_policy policy(label, _config_rom.xml());

			/* check and apply session root offset */
			if (!policy.has_attribute("root")) {
				error("policy lacks 'root' attribute");
				throw Service_denied();
			}
			Root_path const root_path = policy.attribute_value("root", Root_path());
			session_root.import(root_path.string(), "/");

			/*
			 * Determine if the session is writeable.
			 * Policy overrides client argument, both default to false.
			 */
			if (policy.attribute_value("writeable", false))
				writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			/* apply client's root offset. */
			{
				char tmp[MAX_PATH_LEN] { };
				Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
				if (Genode::strcmp("/", tmp, sizeof(tmp))) {
					session_root.append("/");
					session_root.append(tmp);
					session_root.remove_trailing('/');
				}
			}

			/* check if the session root exists */
			if (!((session_root == "/")
			 || _vfs_env.root_dir().directory(session_root.base()))) {
				error("session root '", session_root, "' not found for '", label, "'");
				throw Service_denied();
			}

			Session_component *session = new (md_alloc())
				Session_component(_env, label.string(),
				                  Genode::Ram_quota{ram_quota},
				                  Genode::Cap_quota{cap_quota},
				                  tx_buf_size, _vfs_env.root_dir(),
				                  _progress_handler.pending_nodes,
				                  _progress_handler.pending_sessions,
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

		/**
		 * Session upgrades are important for the VFS server,
		 * this allows sessions to open arbitrarily large amounts
		 * of handles without starving other sessions.
		 */
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
			_env.ep().register_io_progress_handler(_progress_handler);
			_config_rom.sigh(_config_handler);
			env.parent().announce(env.ep().manage(*this));
		}
};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	static Vfs_server::Root root { env, sliced_heap };
}
