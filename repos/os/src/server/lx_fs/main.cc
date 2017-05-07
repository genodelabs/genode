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
#include <file_system/node_handle_registry.h>
#include <file_system_session/rpc_object.h>
#include <os/session_policy.h>
#include <util/xml_node.h>

/* local includes */
#include <directory.h>


namespace File_system {
	struct Main;
	struct Session_component;
	struct Root;
}


class File_system::Session_component : public Session_rpc_object
{
	private:

		Genode::Env          &_env;
		Allocator            &_md_alloc;
		Directory            &_root;
		Node_handle_registry  _handle_registry;
		bool                  _writable;

		Signal_handler<Session_component> _process_packet_dispatcher;


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

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				if (content && (packet.length() <= packet.size()))
					res_length = node.read((char *)content, length, packet.position());
				break;

			case Packet_descriptor::WRITE:
				if (content && (packet.length() <= packet.size()))
					res_length = node.write((char const *)content, length, packet.position());
				break;

			case Packet_descriptor::CONTENT_CHANGED:
				_handle_registry.register_notify(*tx_sink(), packet.handle());
				/* notify_listeners may bounce the packet back*/
				node.notify_listeners();
				/* otherwise defer acknowledgement of this packet */
				return;

			case Packet_descriptor::READ_READY:
				/* not supported */
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

			try {
				Node *node = _handle_registry.lookup_and_lock(packet.handle());
				Node_lock_guard guard(node);

				_process_packet_op(packet, *node);
			}
			catch (Invalid_handle) { Genode::error("Invalid_handle"); }
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

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (!_writable)
				if (create || (mode != STAT_ONLY && mode != READ_ONLY))
					throw Permission_denied();

			File *file = dir->file(name.string(), mode, create);

			Node_lock_guard file_guard(file);
			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{
			Genode::error(__func__, " not implemented");
			return Symlink_handle();
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
			Node_lock_guard guard(dir);
			return _handle_registry.alloc(dir);
		}

		Node_handle node(Path const &path)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			Node *node = _root.node(path_str + 1);

			Node_lock_guard guard(node);
			return _handle_registry.alloc(node);
		}

		void close(Node_handle handle)
		{
			/* FIXME when to destruct node? */
			_handle_registry.free(handle);
		}

		Status status(Node_handle node_handle)
		{
			Node *node = _handle_registry.lookup_and_lock(node_handle);
			Node_lock_guard guard(node);

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

			Genode::error(__func__, " for symlinks not implemented");

			return Status();
		}

		void control(Node_handle, Control)
		{
			Genode::error(__func__, " not implemented");
		}

		void unlink(Dir_handle, Name const &)
		{
			Genode::error(__func__, " not implemented");
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			if (!_writable)
				throw Permission_denied();

			File *file = _handle_registry.lookup_and_lock(file_handle);
			Node_lock_guard file_guard(file);
			file->truncate(size);
		}

		void move(Dir_handle, Name const &, Dir_handle, Name const &)
		{
			Genode::error(__func__, " not implemented");
		}

		/**
		 * We could call sync(2) here but for now we forward just the
		 * reminder because besides testing, there is currently no
		 * use-case.
		 */
		void sync(Node_handle) override { Genode::warning("sync() not implemented!"); }
};


class File_system::Root : public Root_component<Session_component>
{
	private:

		Genode::Env &_env;

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
					 * lookup, we remove all leading slashes.
					 */
					if (root[0] != '/') {
						Genode::error("Root directory must start with / but is \"",
						              Genode::Cstring(root), "\"");
						throw Service_denied();
					}

					for (root_dir = root; *root_dir == '/'; ++root_dir) ;

					/* sanitize possibly empty root_dir to current directory */
					if (*root_dir == 0)
						root_dir = ".";
				} catch (Xml_node::Nonexistent_attribute) {
					Genode::error("missing \"root\" attribute in policy definition");
					throw Service_denied();
				}

				/*
				 * Determine if write access is permitted for the session.
				 */
				writeable = policy.attribute_value("writeable", false);
			}
			catch (Session_policy::No_policy_defined) {
				Genode::error("invalid session request, no matching policy");
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
				Genode::error("insufficient 'ram_quota', "
				              "got ", ram_quota, ", need ", session_size);
				throw Insufficient_ram_quota();
			}

			try {
				return new (md_alloc())
				       Session_component(tx_buf_size, _env, root_dir, writeable, *md_alloc());
			}
			catch (Lookup_failed) {
				Genode::error("session root directory \"", Genode::Cstring(root), "\" "
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


struct File_system::Main
{
	Genode::Env &env;

	Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };

	Root fs_root = { env, sliced_heap };

	Main(Genode::Env &env) : env(env)
	{
		env.parent().announce(env.ep().manage(fs_root));
	}
};


void Component::construct(Genode::Env &env) { static File_system::Main inst(env); }
