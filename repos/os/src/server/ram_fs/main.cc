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
#include <file_system/open_node.h>
#include <file_system_session/rpc_object.h>
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <root/component.h>
#include <os/session_policy.h>

/* local includes */
#include "directory.h"


/*************************
 ** File-system service **
 *************************/

namespace Ram_fs {

	using namespace File_system;
	using File_system::Packet_descriptor;
	using File_system::Path;

	class Session_component;
	class Root;
	class Main;
};


class Ram_fs::Session_component : public File_system::Session_rpc_object
{
	private:

		typedef File_system::Open_node<Node> Open_node;

		Genode::Entrypoint               &_ep;
		Genode::Ram_session              &_ram;
		Genode::Allocator                &_alloc;
		Directory                        &_root;
		Id_space<File_system::Node>       _open_node_registry;
		bool                              _writable;

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
				if (content && (packet.length() <= packet.size())) {
					Locked_ptr<Node> node { open_node.node() };
					if (!node.valid())
						break; 
					res_length = node->read((char *)content, length,
					                        packet.position());
				}
				break;

			case Packet_descriptor::WRITE:
				if (content && (packet.length() <= packet.size())) {
					Locked_ptr<Node> node { open_node.node() };
					if (!node.valid())
						break; 
					res_length = node->write((char const *)content, length,
					                         packet.position());
				}
				open_node.mark_as_written();
				break;

			case Packet_descriptor::CONTENT_CHANGED: {
				open_node.register_notify(*tx_sink());
				Locked_ptr<Node> node { open_node.node() };
				if (!node.valid())
					return; 
				node->notify_listeners();
				return;
			}

			case Packet_descriptor::READ_READY:
				/* not supported */
				break;

			case Packet_descriptor::SYNC: {
				Locked_ptr<Node> node { open_node.node() };
				if (!node.valid())
					break; 
				node->notify_listeners();
				break;
			}
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
		 * Check if string represents a valid path (most start with '/')
		 */
		static void _assert_valid_path(char const *path)
		{
			if (!path || path[0] != '/') {
				Genode::warning("malformed path '", Genode::Cstring(path), "'");
				throw Lookup_failed();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(size_t tx_buf_size, Genode::Entrypoint &ep,
		                  Genode::Ram_session &ram, Genode::Region_map &rm,
		                  Genode::Allocator &alloc,
		                  Directory &root, bool writable)
		:
			Session_rpc_object(ram.alloc(tx_buf_size), rm, ep.rpc_ep()),
			_ep(ep),
			_ram(ram),
			_alloc(alloc),
			_root(root),
			_writable(writable),
			_process_packet_handler(_ep, *this, &Session_component::_process_packets)
		{
			/*
			 * Register '_process_packets' method as signal handler for
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
			Dataspace_capability ds = tx_sink()->dataspace();
			_ram.free(static_cap_cast<Ram_dataspace>(ds));
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name,
		                 Mode mode, bool create)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			auto file_fn = [&] (Open_node &open_node) {

				Locked_ptr<Node> dir { open_node.node() };

				if (!dir.valid())
					throw Unavailable();

				if (!_writable)
					if (mode != STAT_ONLY && mode != READ_ONLY)
						throw Permission_denied();

				if (create) {

					if (!_writable)
						throw Permission_denied();

					if (dir->has_sub_node_unsynchronized(name.string()))
						throw Node_already_exists();

					try {
						File * const file = new (_alloc)
							File(_alloc, name.string());

						dir->adopt_unsynchronized(file);
						open_node.mark_as_written();
					}
					catch (Allocator::Out_of_memory) { throw No_space(); }
				}

				File *file = dir->lookup_file(name.string());

				Open_node *open_file =
					new (_alloc) Open_node(file->weak_ptr(), _open_node_registry);

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
			if (!valid_name(name.string()))
				throw Invalid_name();

			auto symlink_fn = [&] (Open_node &open_node) {

				Locked_ptr<Node> dir { open_node.node() };

				if (!dir.valid())
					throw Unavailable();

				if (create) {

					if (!_writable)
						throw Permission_denied();

					if (dir->has_sub_node_unsynchronized(name.string()))
						throw Node_already_exists();

					try {
						Symlink * const symlink = new (_alloc)
					                    	Symlink(name.string());

						dir->adopt_unsynchronized(symlink);
					}
					catch (Allocator::Out_of_memory) { throw No_space(); }
				}

				Symlink *symlink = dir->lookup_symlink(name.string());

				Open_node *open_symlink =
					new (_alloc) Open_node(symlink->weak_ptr(), _open_node_registry);

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

			if (create) {

				if (!_writable)
					throw Permission_denied();

				if (!path.valid_string())
					throw Name_too_long();

				Directory *parent = _root.lookup_parent(path_str);

				char const *name = basename(path_str);

				if (parent->has_sub_node_unsynchronized(name))
					throw Node_already_exists();

				try {
					parent->adopt_unsynchronized(new (_alloc) Directory(name));
				} catch (Allocator::Out_of_memory) {
					throw No_space();
				}
			}

			Directory *dir = _root.lookup_dir(path_str);

			Open_node *open_dir =
				new (_alloc) Open_node(dir->weak_ptr(), _open_node_registry);

			return Dir_handle { open_dir->id().value };
		}

		Node_handle node(Path const &path)
		{
			_assert_valid_path(path.string());

			Node *node = _root.lookup(path.string() + 1);

			Open_node *open_node =
				new (_alloc) Open_node(node->weak_ptr(), _open_node_registry);

			return open_node->id();
		}

		void close(Node_handle handle)
		{
			auto close_fn = [&] (Open_node &open_node) {
				destroy(_alloc, &open_node);
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
				Locked_ptr<Node> node { open_node.node() };
				if (!node.valid())
					throw Unavailable();	
				return node->status();
			};

			try {
				return _open_node_registry.apply<Open_node>(node_handle, status_fn);
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		void control(Node_handle, Control) { }

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!valid_name(name.string()))
				throw Invalid_name();

			if (!_writable)
				throw Permission_denied();

			auto unlink_fn = [&] (Open_node &open_node) {

				Locked_ptr<Node> dir { open_node.node() };

				if (!dir.valid())
					throw Unavailable();

				Node *node = dir->lookup(name.string());

				dir->discard(node);

				destroy(_alloc, node);
				open_node.mark_as_written();
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
				Locked_ptr<Node> node { open_node.node() };
				if (!node.valid())
					throw Unavailable();
				node->truncate(size);
				open_node.mark_as_written();
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
			if (!_writable)
				throw Permission_denied();

			if (!valid_name(from_name.string()))
				throw Lookup_failed();

			if (!valid_name(to_name.string()))
				throw Invalid_name();

			auto move_fn = [&] (Open_node &open_from_dir_node) {

				auto inner_move_fn = [&] (Open_node &open_to_dir_node) {

					Locked_ptr<Node> from_dir { open_from_dir_node.node() };

					if (!from_dir.valid())
						throw Unavailable();

					Node *node = from_dir->lookup(from_name.string());
					node->name(to_name.string());

					if (!(open_to_dir_node.node() == open_from_dir_node.node())) {

						Locked_ptr<Node> to_dir { open_to_dir_node.node() };

						if (!to_dir.valid())
							throw Unavailable();

						from_dir->discard(node);
						to_dir->adopt_unsynchronized(node);

						/*
						 * If the file was moved from one directory to another we
						 * need to inform the new directory 'to_dir'. The original
						 * directory 'from_dir' will always get notified (i.e.,
						 * when just the file name was changed) below.
						 */
						to_dir->mark_as_updated();
						open_to_dir_node.mark_as_written();
						to_dir->notify_listeners();

						from_dir->mark_as_updated();
						open_from_dir_node.mark_as_written();
						from_dir->notify_listeners();

						node->mark_as_updated();
						node->notify_listeners();
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


class Ram_fs::Root : public Root_component<Session_component>
{
	private:

		Genode::Entrypoint    &_ep;
		Genode::Allocator     &_alloc;
		Genode::Ram_session   &_ram;
		Genode::Region_map    &_rm;
		Genode::Xml_node const _config;
		Directory             &_root_dir;

	protected:

		Session_component *_create_session(const char *args)
		{
			/*
			 * Determine client-specific policy defined implicitly by
			 * the client's label.
			 */

			enum { ROOT_MAX_LEN = 256 };
			Genode::Path<MAX_PATH_LEN> session_root;
			char tmp[MAX_PATH_LEN];

			Directory *session_root_dir = nullptr;
			bool writeable = false;

			Session_label const label = label_from_args(args);
			try {
				Session_policy policy(label, _config);

				/*
				 * Determine directory that is used as root directory of
				 * the session. Clients without a specified root are denied.
				 */
				try {
					policy.attribute("root").value(tmp, sizeof(tmp));
					session_root.import(tmp, "/");
				} catch (Xml_node::Nonexistent_attribute) {
					Genode::error("missing \"root\" attribute in policy definition");
					throw Service_denied();
				}

				/*
				 * Determine if the session is writeable.
				 * Policy overrides client argument, both default to false.
				 */
				if (policy.attribute_value("writeable", false))
					writeable = Arg_string::find_arg(args, "writeable").bool_value(false);

			} catch (Session_policy::No_policy_defined) {
				Genode::error("invalid session request, no matching policy");
				throw Service_denied();
			}

			/* apply client's root offset */
			Arg_string::find_arg(args, "root").string(tmp, sizeof(tmp), "/");
			if (Genode::strcmp("/", tmp, sizeof(tmp))) {
				session_root.append("/");
				session_root.append(tmp);
			}
			session_root.remove_trailing('/');
			if (session_root == "/") {
				session_root_dir = &_root_dir;
			} else {
				try {
					/*
					 * The root path is specified with a leading path
					 * delimiter. For performing the lookup, we skip the first
					 * character.
					 */
					session_root_dir = _root_dir.lookup_dir(
						session_root.base() + 1);
				}
				catch (Lookup_failed) { throw Service_denied(); }
			}

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).aligned_size();
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").aligned_size();

			if (!tx_buf_size) {
				Genode::error(label, " requested a session with a zero length transmission buffer");
				throw Service_denied();
			}

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((size_t)4096, session_size) > ram_quota) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", session_size);
				throw Insufficient_ram_quota();
			}
			return new (md_alloc())
				Session_component(tx_buf_size, _ep, _ram, _rm, _alloc,
				                  *session_root_dir, writeable);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep        entrypoint
		 * \param md_alloc  meta-data allocator
		 * \param alloc     general-purpose allocator
		 * \param root_dir  root-directory handle (anchor for fs)
		 */
		Root(Genode::Entrypoint &ep, Genode::Ram_session &ram,
		     Genode::Region_map &rm, Genode::Xml_node config,
		     Allocator &md_alloc, Allocator &alloc, Directory &root_dir)
		:
			Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep), _alloc(alloc), _ram(ram), _rm(rm), _config(config),
			_root_dir(root_dir)
		{ }
};


/**
 * Helper for conveniently accessing 'Xml_node' attribute strings
 */
struct Attribute_string
{
	char buf[File_system::MAX_NAME_LEN];

	/**
	 * Constructor
	 *
	 * \param attr      attribute name
	 * \param fallback  if non-null, this is the string used if the attribute
	 *                  is not defined. If null, the constructor throws
	 *                  an 'Nonexistent_attribute' exception'
	 * \throw Xml_node::Nonexistent_attribute
	 */
	Attribute_string(Genode::Xml_node node, char const *attr, char *fallback = 0)
	{
		try { node.attribute(attr).value(buf, sizeof(buf)); }
		catch (Genode::Xml_node::Nonexistent_attribute) {

			if (fallback) {
				Genode::strncpy(buf, fallback, sizeof(buf));
			} else {
				char type_name[16];
				node.type_name(type_name, sizeof(type_name));
				Genode::warning("missing \"", attr, "\" attribute in "
				                "<", Genode::Cstring(type_name), "> node");
				throw Genode::Xml_node::Nonexistent_attribute();
			}
		}
	}

	operator char * () { return buf; }

	void print(Genode::Output &out) const { Genode::print(out, Genode::Cstring(buf)); }
};


static void preload_content(Genode::Env            &env,
                            Genode::Allocator      &alloc,
                            Genode::Xml_node        node,
                            Ram_fs::Directory &dir)
{
	using namespace File_system;

	for (unsigned i = 0; i < node.num_sub_nodes(); i++) {
		Xml_node sub_node = node.sub_node(i);

		/*
		 * Lookup name attribtue, let 'Nonexistent_attribute' exception fall
		 * through because this configuration error is considered fatal.
		 */
		Attribute_string name(sub_node, "name");

		/*
		 * Create directory
		 */
		if (sub_node.has_type("dir")) {

			Ram_fs::Directory *sub_dir = new (&alloc) Ram_fs::Directory(name);

			/* traverse into the new directory */
			preload_content(env, alloc, sub_node, *sub_dir);

			dir.adopt_unsynchronized(sub_dir);
		}

		/*
		 * Create file from ROM module
		 */
		if (sub_node.has_type("rom")) {

			/* read "as" attribute, use "name" as default */
			Attribute_string as(sub_node, "as", name);

			/* read file content from ROM module */
			try {
				Attached_rom_dataspace rom(env, name);

				Ram_fs::File *file = new (&alloc) Ram_fs::File(alloc, as);
				file->write(rom.local_addr<char>(), rom.size(), 0);
				dir.adopt_unsynchronized(file);
			}
			catch (Rom_connection::Rom_connection_failed) {
				Genode::warning("failed to open ROM module \"", name, "\""); }
			catch (Region_map::Region_conflict) {
				Genode::warning("Could not locally attach ROM module \"", name, "\""); }
		}

		/*
		 * Create file from inline data provided as content of the XML node
		 */
		if (sub_node.has_type("inline")) {

			Ram_fs::File *file = new (&alloc) Ram_fs::File(alloc, name);
			file->write(sub_node.content_addr(), sub_node.content_size(), 0);
			dir.adopt_unsynchronized(file);
		}
	}
}


struct Ram_fs::Main
{
	Genode::Env &_env;

	Directory _root_dir { "" };

	Genode::Attached_rom_dataspace _config { _env, "config" };

	/*
	 * Initialize root interface
	 */
	Genode::Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

	Genode::Heap _heap { _env.ram(), _env.rm() };

	Root _fs_root { _env.ep(), _env.ram(), _env.rm(), _config.xml(),
	                _sliced_heap, _heap, _root_dir };

	Main(Genode::Env &env) : _env(env)
	{
		/* preload RAM file system with content as declared in the config */
		try {
			preload_content(_env, _heap, _config.xml().sub_node("content"), _root_dir); }
		catch (Xml_node::Nonexistent_sub_node) { }

		_env.parent().announce(_env.ep().manage(_fs_root));
	}
};


void Component::construct(Genode::Env &env) { static Ram_fs::Main inst(env); }
