/*
 * \brief  RAM file system
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system/node_handle_registry.h>
#include <file_system_session/rpc_object.h>
#include <base/heap.h>
#include <root/component.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <util/xml_node.h>

/* local includes */
#include <ram_fs/directory.h>


/*************************
 ** File-system service **
 *************************/

namespace File_system {

	class Session_component : public Session_rpc_object
	{
		private:

			Server::Entrypoint   &_ep;
			Directory            &_root;
			Node_handle_registry  _handle_registry;
			bool                  _writable;

			Signal_rpc_member<Session_component> _process_packet_dispatcher;


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
					Node *node = _handle_registry.lookup_and_lock(packet.handle());
					Node_lock_guard guard(node);

					_process_packet_op(packet, *node);
				}
				catch (Invalid_handle)     { Genode::error("Invalid_handle");     }

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
			 * Check if string represents a valid path (most start with '/')
			 */
			static void _assert_valid_path(char const *path)
			{
				if (!path || path[0] != '/') {
					Genode::warning("malformed path ''", path, "'");
					throw Lookup_failed();
				}
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(size_t tx_buf_size, Server::Entrypoint &ep,
			                  Directory &root, bool writable)
			:
				Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
				_ep(ep),
				_root(root),
				_writable(writable),
				_process_packet_dispatcher(ep, *this, &Session_component::_process_packets)
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

			File_handle file(Dir_handle dir_handle, Name const &name,
			                 Mode mode, bool create)
			{
				if (!valid_name(name.string()))
					throw Invalid_name();

				Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
				Node_lock_guard dir_guard(dir);

				if (!_writable)
					if (mode != STAT_ONLY && mode != READ_ONLY)
						throw Permission_denied();

				if (create) {

					if (!_writable)
						throw Permission_denied();

					if (dir->has_sub_node_unsynchronized(name.string()))
						throw Node_already_exists();

					try {
						File * const file = new (env()->heap())
						                    File(*env()->heap(), name.string());

						dir->adopt_unsynchronized(file);
					}
					catch (Allocator::Out_of_memory) { throw No_space(); }
				}

				File *file = dir->lookup_and_lock_file(name.string());
				Node_lock_guard file_guard(file);
				return _handle_registry.alloc(file);
			}

			Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
			{
				if (!valid_name(name.string()))
					throw Invalid_name();

				Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
				Node_lock_guard dir_guard(dir);

				if (create) {

					if (!_writable)
						throw Permission_denied();

					if (dir->has_sub_node_unsynchronized(name.string()))
						throw Node_already_exists();

					try {
						Symlink * const symlink = new (env()->heap())
						                    Symlink(name.string());

						dir->adopt_unsynchronized(symlink);
					}
					catch (Allocator::Out_of_memory) { throw No_space(); }
				}

				Symlink *symlink = dir->lookup_and_lock_symlink(name.string());
				Node_lock_guard file_guard(symlink);
				return _handle_registry.alloc(symlink);
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

					Directory *parent = _root.lookup_and_lock_parent(path_str);

					Node_lock_guard guard(parent);

					char const *name = basename(path_str);

					if (parent->has_sub_node_unsynchronized(name))
						throw Node_already_exists();

					try {
						parent->adopt_unsynchronized(new (env()->heap()) Directory(name));
					} catch (Allocator::Out_of_memory) {
						throw No_space();
					}
				}

				Directory *dir = _root.lookup_and_lock_dir(path_str);
				Node_lock_guard guard(dir);
				return _handle_registry.alloc(dir);
			}

			Node_handle node(Path const &path)
			{
				_assert_valid_path(path.string());

				Node *node = _root.lookup_and_lock(path.string() + 1);

				Node_lock_guard guard(node);
				return _handle_registry.alloc(node);
			}

			void close(Node_handle handle)
			{
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
				Symlink *symlink = dynamic_cast<Symlink *>(node);
				if (symlink) {
					s.size = symlink->length();
					s.mode = File_system::Status::MODE_SYMLINK;
					return s;
				}
				return Status();
			}

			void control(Node_handle, Control) { }

			void unlink(Dir_handle dir_handle, Name const &name)
			{
				if (!valid_name(name.string()))
					throw Invalid_name();

				if (!_writable)
					throw Permission_denied();

				Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
				Node_lock_guard dir_guard(dir);

				Node *node = dir->lookup_and_lock(name.string());

				dir->discard_unsynchronized(node);

				// XXX implement ref counting, do not destroy node that is
				//     is still referenced by a node handle

				node->unlock();
				destroy(env()->heap(), node);
			}

			void truncate(File_handle file_handle, file_size_t size)
			{
				if (!_writable)
					throw Permission_denied();

				File *file = _handle_registry.lookup_and_lock(file_handle);
				Node_lock_guard file_guard(file);
				file->truncate(size);
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

				Directory *from_dir = _handle_registry.lookup_and_lock(from_dir_handle);
				Node_lock_guard from_dir_guard(from_dir);

				Node *node = from_dir->lookup_and_lock(from_name.string());
				Node_lock_guard node_guard(node);
				node->name(to_name.string());

				if (!_handle_registry.refer_to_same_node(from_dir_handle, to_dir_handle)) {
					Directory *to_dir = _handle_registry.lookup_and_lock(to_dir_handle);
					Node_lock_guard to_dir_guard(to_dir);

					from_dir->discard_unsynchronized(node);
					to_dir->adopt_unsynchronized(node);

					/*
					 * If the file was moved from one directory to another we
					 * need to inform the new directory 'to_dir'. The original
					 * directory 'from_dir' will always get notified (i.e.,
					 * when just the file name was changed) below.
					 */
					to_dir->mark_as_updated();
					to_dir->notify_listeners();
				}

				from_dir->mark_as_updated();
				from_dir->notify_listeners();

				node->mark_as_updated();
				node->notify_listeners();
			}

			void sigh(Node_handle node_handle, Signal_context_capability sigh)
			{
				_handle_registry.sigh(node_handle, sigh);
			}
	};


	class Root : public Root_component<Session_component>
	{
		private:

			Server::Entrypoint &_ep;
			Directory          &_root_dir;

		protected:

			Session_component *_create_session(const char *args)
			{
				/*
				 * Determine client-specific policy defined implicitly by
				 * the client's label.
				 */

				Directory *session_root_dir = 0;
				bool writeable = false;

				enum { ROOT_MAX_LEN = 256 };
				char root[ROOT_MAX_LEN];
				root[0] = 0;

				Session_label const label = label_from_args(args);
				try {
					Session_policy policy(label);

					/*
					 * Determine directory that is used as root directory of
					 * the session.
					 */
					try {
						policy.attribute("root").value(root, sizeof(root));
						if (strcmp("/", root) == 0) {
							session_root_dir = &_root_dir;
						} else {

							/*
							 * Make sure the root path is specified with a
							 * leading path delimiter. For performing the
							 * lookup, we skip the first character.
							 */
							if (root[0] != '/')
								throw Lookup_failed();

							session_root_dir = _root_dir.lookup_and_lock_dir(root + 1);
							session_root_dir->unlock();
						}
					} catch (Xml_node::Nonexistent_attribute) {
						Genode::error("missing \"root\" attribute in policy definition");
						throw Root::Unavailable();
					} catch (Lookup_failed) {
						Genode::error("session root directory \"",
						              Genode::Cstring(root), "\" does not exist");
						throw Root::Unavailable();
					}

					/*
					 * Determine if write access is permitted for the session.
					 */
					writeable = policy.attribute_value("writeable", false);

				} catch (Session_policy::No_policy_defined) {
					Genode::error("invalid session request, no matching policy");
					throw Root::Unavailable();
				}

				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
				size_t tx_buf_size =
					Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

				if (!tx_buf_size) {
					Genode::error(label, " requested a session with a zero length transmission buffer");
					throw Root::Invalid_args();
				}

				/*
				 * Check if donated ram quota suffices for session data,
				 * and communication buffer.
				 */
				size_t session_size = sizeof(Session_component) + tx_buf_size;
				if (max((size_t)4096, session_size) > ram_quota) {
					Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
					              "need ", session_size);
					throw Root::Quota_exceeded();
				}
				return new (md_alloc())
					Session_component(tx_buf_size, _ep, *session_root_dir, writeable);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param ep        entrypoint
			 * \param md_alloc  meta-data allocator
			 * \param root_dir  root-directory handle (anchor for fs)
			 */
			Root(Server::Entrypoint &ep, Allocator &md_alloc, Directory &root_dir)
			:
				Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
				_ep(ep),
				_root_dir(root_dir)
			{ }
	};

	struct Main;
}


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
		try {
			node.attribute(attr).value(buf, sizeof(buf));
		} catch (Genode::Xml_node::Nonexistent_attribute) {

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


static void preload_content(Genode::Allocator      &alloc,
                            Genode::Xml_node        node,
                            File_system::Directory &dir)
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

			Directory *sub_dir = new (&alloc) Directory(name);

			/* traverse into the new directory */
			preload_content(alloc, sub_node, *sub_dir);

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
				Attached_rom_dataspace rom(name);
				File *file = new (&alloc) File(alloc, as);
				file->write(rom.local_addr<char>(), rom.size(), 0);
				dir.adopt_unsynchronized(file);
			}
			catch (Rom_connection::Rom_connection_failed) {
				Genode::warning("failed to open ROM file \"", name, "\""); }
			catch (Rm_session::Attach_failed) {
				Genode::warning("Could not locally attach ROM file \"", name, "\""); }
		}

		/*
		 * Create file from inline data provided as content of the XML node
		 */
		if (sub_node.has_type("inline")) {

			File *file = new (&alloc) File(alloc, name);
			file->write(sub_node.content_addr(), sub_node.content_size(), 0);
			dir.adopt_unsynchronized(file);
		}
	}
}


struct File_system::Main
{
	Server::Entrypoint &ep;

	Directory root_dir = { "" };

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root = { ep, sliced_heap, root_dir };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		/* preload RAM file system with content as declared in the config */
		try {
			Xml_node content = config()->xml_node().sub_node("content");
			preload_content(*env()->heap(), content, root_dir); }
		catch (Xml_node::Nonexistent_sub_node) { }

		env()->parent()->announce(ep.manage(fs_root));
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "ram_fs_ep"; }
Genode::size_t Server::stack_size()                      { return 2048 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static File_system::Main inst(ep); }
