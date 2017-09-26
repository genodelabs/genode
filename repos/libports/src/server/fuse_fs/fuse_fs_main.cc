/*
 * \brief  FUSE file system
 * \author Josef Soentgen
 * \date   2013-11-27
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <file_system_session/rpc_object.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/xml_node.h>
#include <libc/component.h>

/* libc includes */
#include <errno.h>

/* local includes */
#include <directory.h>
#include <open_node.h>
#include <util.h>


namespace Fuse_fs {

	using namespace Genode;
	using File_system::Packet_descriptor;
	using File_system::Path;

	struct Main;
	struct Session_component;
	struct Root;
}


class Fuse_fs::Session_component : public Session_rpc_object
{
	private:

		typedef File_system::Open_node<Node> Open_node;

		Genode::Env                 &_env;
		Allocator                   &_md_alloc;
		Directory                   &_root;
		Id_space<File_system::Node>  _open_node_registry;
		bool                         _writeable;

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
				Fuse::sync_fs();
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
		 * Called by signal handler, executed in the context of the main
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
			if (!path || path[0] != '/') {
				Genode::warning("malformed path '", path, "'");
				throw Lookup_failed();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(size_t      tx_buf_size,
		                  Genode::Env &env,
		                  char const *root_dir,
		                  bool        writeable,
		                  Allocator  &md_alloc)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.rm(), env.ep().rpc_ep()),
			_env(env),
			_md_alloc(md_alloc),
			_root(*new (&_md_alloc) Directory(_md_alloc, root_dir, false)),
			_writeable(writeable),
			_process_packet_handler(_env.ep(), *this, &Session_component::_process_packets)
		{
			_tx.sigh_packet_avail(_process_packet_handler);
			_tx.sigh_ready_to_ack(_process_packet_handler);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			Fuse::sync_fs();

			Dataspace_capability ds = tx_sink()->dataspace();
			_env.ram().free(static_cap_cast<Ram_dataspace>(ds));
			destroy(&_md_alloc, &_root);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_filename(name.string()))
				throw Invalid_name();

			auto file_fn = [&] (Open_node &open_node) {

				Node &dir = open_node.node();

				if (create && !_writeable)
					throw Permission_denied();

				File *file = new (&_md_alloc) File(&dir, name.string(), mode, create);

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
			if (! Fuse::support_symlinks()) {
				Genode::error("FUSE file system does not support symlinks");
				throw Permission_denied();
			}

			if (!valid_filename(name.string()))
			    throw Invalid_name();

			auto symlink_fn = [&] (Open_node &open_node) {

				Node &dir = open_node.node();

				if (create && !_writeable)
						throw Permission_denied();

				Symlink *symlink = new (&_md_alloc) Symlink(&dir, name.string(), create);

				Open_node *open_symlink =
					new (_md_alloc) Open_node(*symlink, _open_node_registry);

				return Symlink_handle { open_symlink->id().value };
			};

			try {
				return _open_node_registry.apply<Open_node>(dir_handle, symlink_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		Dir_handle dir(Path const &path, bool create)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			if (create && !_writeable)
				throw Permission_denied();

			if (!path.valid_string())
				throw Name_too_long();

			Directory *dir_node = new (&_md_alloc) Directory(_md_alloc, path_str, create);

			Open_node *open_dir =
				new (_md_alloc) Open_node(*dir_node, _open_node_registry);

			return Dir_handle { open_dir->id().value };
		}

		Node_handle node(Path const &path)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			/**
			 * FIXME this leads to '/' as parent and 'the rest' as name,
			 * which fortunatly is in this case not a problem.
			 */
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

		void control(Node_handle, Control)
		{
			Genode::error(__func__, " not implemented");
		}

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!_writeable)
				throw Permission_denied();

			auto unlink_fn = [&] (Open_node &open_node) {

				Node &dir = open_node.node();

				Absolute_path absolute_path(_root.name());

				try {
					absolute_path.append(dir.name());
					absolute_path.append("/");
					absolute_path.append(name.string());
				} catch (Path_base::Path_too_long) {
					throw Invalid_name();
				}

				/* XXX remove direct use of FUSE operations */
				int res = Fuse::fuse()->op.unlink(absolute_path.base());

				if (res != 0) {
					Genode::error("fuse()->op.unlink() returned unexpected error code: ", res);
					return;
				}
			};

			try {
				_open_node_registry.apply<Open_node>(dir_handle, unlink_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			if (!_writeable)
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
			  Dir_handle to_dir_handle,   Name const &to_name)
		{
			if (!_writeable)
				throw Permission_denied();
			
			auto move_fn = [&] (Open_node &open_from_dir_node) {

				auto inner_move_fn = [&] (Open_node &open_to_dir_node) {

					Node &from_dir = open_from_dir_node.node();
					Node &to_dir = open_to_dir_node.node();

					Absolute_path absolute_from_path(_root.name());
					Absolute_path absolute_to_path(_root.name());

					try {
						absolute_from_path.append(from_dir.name());
						absolute_from_path.append("/");
						absolute_from_path.append(from_name.string());
						absolute_to_path.append(to_dir.name());
						absolute_to_path.append("/");
						absolute_to_path.append(to_name.string());
					} catch (Path_base::Path_too_long) {
						throw Invalid_name();
					}

					/* XXX remove direct use of FUSE operations */
					int res = Fuse::fuse()->op.rename(absolute_to_path.base(),
			                                  	  	  absolute_from_path.base());

					if (res != 0) {
						Genode::error("fuse()->op.rename() returned unexpected error code: ", res);
						return;
					}
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


class Fuse_fs::Root : public Root_component<Session_component>
{
	private:

		Genode::Env                   &_env;
		Genode::Attached_rom_dataspace _config { _env, "config" };

	protected:

		Session_component *_create_session(const char *args)
		{
			/*
			 * Determine client-specific policy defined implicitly by
			 * the client's label.
			 */

			char const *root_dir  = ".";
			bool        writeable = false;

			enum { ROOT_MAX_LEN = 256 };
			char root[ROOT_MAX_LEN];
			root[0] = 0;

			Session_label const label = label_from_args(args);
			try {
				Session_policy policy(label, _config.xml());

				/*
				 * Determine directory that is used as root directory of
				 * the session.
				 */
				try {
					policy.attribute("root").value(root, sizeof(root));

					/*
					 * Make sure the root path is specified with a
					 * leading path delimiter. For performing the
					 * lookup, we skip the first character.
					 */
					if (root[0] != '/')
						throw Lookup_failed();

					root_dir = root;
				}
				catch (Xml_node::Nonexistent_attribute) {
					Genode::error("missing \"root\" attribute in policy definition");
					throw Service_denied();
				}
				catch (Lookup_failed) {
					Genode::error("session root directory \"",
					              Genode::Cstring(root), "\" does not exist");
					throw Service_denied();
				}

				/*
				 * Determine if write access is permitted for the session.
				 */
				writeable = policy.attribute_value("writeable", false);
				if (writeable)
					Genode::warning("write support in fuse_fs is considered experimental, data-loss may occur.");

			} catch (Session_policy::No_policy_defined) {
				Genode::error("Invalid session request, no matching policy");
				throw Genode::Service_denied();
			}

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			if (!tx_buf_size) {
				Genode::error(label, " requested a session with a zero length transmission buffer");
				throw Genode::Service_denied();
			}

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((size_t)4096, session_size) > ram_quota) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, " , "
				              "need ", session_size);
				throw Insufficient_ram_quota();
			}
			return new (md_alloc())
				Session_component(tx_buf_size, _env, root_dir, writeable, *md_alloc());
		}

	public:

		/**
		 * Constructor
		 *
		 * \param env         environment
		 * \param md_alloc    meta-data allocator
		 */
		Root(Genode::Env & env, Allocator &md_alloc)
		: Root_component<Session_component>(env.ep(), md_alloc),
		  _env(env) { }
};


struct Fuse_fs::Main
{
	Genode::Env & env;
	Sliced_heap   sliced_heap { env.ram(), env.rm() };
	Root          fs_root     { env, sliced_heap    };

	Main(Genode::Env & env) : env(env)
	{
		if (!Fuse::init_fs()) {
			Genode::error("FUSE fs initialization failed");
			return;
		}

		env.parent().announce(env.ep().manage(fs_root));
	}

	~Main()
	{
		if (Fuse::initialized()) {
			Fuse::deinit_fs();
		}
	}
};


/***************
 ** Component **
 ***************/

void Libc::Component::construct(Libc::Env &env)
{
	static Fuse_fs::Main inst(env);
}

