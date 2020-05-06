/*
 * \brief  RAM file system
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <root/component.h>
#include <file_system_session/rpc_object.h>
#include <os/session_policy.h>
#include <util/xml_node.h>

/* local includes */
#include <directory.h>
#include <open_node.h>

namespace Lx_fs {

	using namespace File_system;
	using File_system::Packet_descriptor;
	using File_system::Path;

	struct Main;
	struct Session_component;
	struct Root;
}


class Lx_fs::Session_component : public Session_rpc_object
{
	private:

		typedef File_system::Open_node<Node> Open_node;

		Genode::Env                 &_env;
		Allocator                   &_md_alloc;
		Directory                   &_root;
		Id_space<File_system::Node>  _open_node_registry { };
		bool                         _writable;

		Signal_handler<Session_component> _process_packet_dispatcher;


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
			size_t     const length  = packet.length();

			/* resulting length */
			size_t res_length = 0;
			bool succeeded = false;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				if (tx_sink()->packet_valid(packet) && (packet.length() <= packet.size())) {
					res_length = open_node.node().read((char *)tx_sink()->packet_content(packet), length,
					                                   packet.position());

					/* read data or EOF is a success */
					succeeded = res_length || (packet.position() >= open_node.node().status().size);
				}
				break;

			case Packet_descriptor::WRITE:
				if (tx_sink()->packet_valid(packet) && (packet.length() <= packet.size())) {
					res_length = open_node.node().write((char const *)tx_sink()->packet_content(packet),
					                                    length,
					                                    packet.position());

					/* File system session can't handle partial writes */
					if (res_length != length) {
						Genode::error("partial write detected ",
						              res_length, " vs ", length);
						/* don't acknowledge */
						return;
					}
					succeeded = true;
				}
				break;

			case Packet_descriptor::WRITE_TIMESTAMP:
				if (tx_sink()->packet_valid(packet) && (packet.length() <= packet.size())) {

					packet.with_timestamp([&] (File_system::Timestamp const time) {
						open_node.node().update_modification_time(time);
						succeeded = true;
					});
				}
				break;

			case Packet_descriptor::CONTENT_CHANGED:
				open_node.register_notify(*tx_sink());
				/* notify_listeners may bounce the packet back*/
				open_node.node().notify_listeners();
				/* otherwise defer acknowledgement of this packet */
				return;

			case Packet_descriptor::READ_READY:
				succeeded = true;
				/* not supported */
				break;

			case Packet_descriptor::SYNC:

				if (tx_sink()->packet_valid(packet)) {
					succeeded = open_node.node().sync();
				}

				break;
			}

			packet.length(res_length);
			packet.succeeded(succeeded);
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
			if (!path || path[0] != '/') {
				Genode::warning("malformed path '", path, "'");
				throw Lookup_failed();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(size_t       tx_buf_size,
		                  Genode::Env &env,
		                  char const  *root_dir,
		                  bool         writable,
		                  Allocator   &md_alloc)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size), env.rm(), env.ep().rpc_ep()),
			_env(env),
			_md_alloc(md_alloc),
			_root(*new (&_md_alloc) Directory(_md_alloc, root_dir, false)),
			_writable(writable),
			_process_packet_dispatcher(env.ep(), *this, &Session_component::_process_packets)
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
			_env.ram().free(static_cap_cast<Ram_dataspace>(ds));
			destroy(&_md_alloc, &_root);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create) override
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

		Symlink_handle symlink(Dir_handle, Name const &, bool /* create */) override
		{
			Genode::error(__func__, " not implemented");
			throw Permission_denied();
		}

		Dir_handle dir(Path const &path, bool create) override
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

		Node_handle node(Path const &path) override
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			Node *node = _root.node(path_str + 1);

			Open_node *open_node =
				new (_md_alloc) Open_node(*node, _open_node_registry);

			return open_node->id();
		}

		void close(Node_handle handle) override
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

		Status status(Node_handle node_handle) override
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

		void control(Node_handle, Control) override
		{
			Genode::error(__func__, " not implemented");
		}

		void unlink(Dir_handle, Name const &) override
		{
			Genode::error(__func__, " not implemented");
		}

		void truncate(File_handle file_handle, file_size_t size) override
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

		void move(Dir_handle dir_from, Name const & name_from,
		          Dir_handle dir_to,   Name const & name_to) override
		{
			typedef File_system::Open_node<Directory> Dir_node;

			Directory *to = 0;

			auto to_fn = [&] (Dir_node &dir_node) {
				to = &dir_node.node();
			};

			try {
				_open_node_registry.apply<Dir_node>(dir_to, to_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}

			auto move_fn = [&] (Dir_node &dir_node) {
				dir_node.node().rename(*to, name_from.string(), name_to.string());
			};

			try {
				_open_node_registry.apply<Dir_node>(dir_from, move_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}
};


class Lx_fs::Root : public Root_component<Session_component>
{
	private:

		Genode::Env &_env;

		Genode::Attached_rom_dataspace _config { _env, "config" };

		static inline bool writeable_from_args(char const *args)
		{
			return { Arg_string::find_arg(args, "writeable").bool_value(true) };
		}

	protected:

		Session_component *_create_session(const char *args) override
		{
			/*
			 * Determine client-specific policy defined implicitly by
			 * the client's label.
			 */

			char const *root_dir  = ".";
			bool        writeable = false;

			Session_label  const label = label_from_args(args);
			Session_policy const policy(label, _config.xml());

			if (!policy.has_attribute("root")) {
				Genode::error("missing \"root\" attribute in policy definition");
				throw Service_denied();
			}

			/*
			 * Determine directory that is used as root directory of
			 * the session.
			 */
			typedef String<256> Root;
			Root const root = policy.attribute_value("root", Root());

			/*
			 * Make sure the root path is specified with a
			 * leading path delimiter. For performing the
			 * lookup, we remove all leading slashes.
			 */
			if (root.string()[0] != '/') {
				Genode::error("Root directory must start with / but is \"", root, "\"");
				throw Service_denied();
			}

			for (root_dir = root.string(); *root_dir == '/'; ++root_dir) ;

			/* sanitize possibly empty root_dir to current directory */
			if (*root_dir == 0)
				root_dir = ".";

			/*
			 * Determine if write access is permitted for the session.
			 */
			writeable = policy.attribute_value("writeable", false) &&
			            writeable_from_args(args);

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
				Genode::error("insufficient 'ram_quota', "
				              "got ", ram_quota, ", need ", session_size);
				throw Insufficient_ram_quota();
			}

			try {
				return new (md_alloc())
				       Session_component(tx_buf_size, _env, root_dir, writeable, *md_alloc());
			}
			catch (Lookup_failed) {
				Genode::error("session root directory \"", root, "\" "
				              "does not exist");
				throw Service_denied();
			}
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{ }
};


struct Lx_fs::Main
{
	Genode::Env &env;

	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root fs_root = { env, sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(fs_root));
	}
};


void Component::construct(Genode::Env &env) { static Lx_fs::Main inst(env); }
