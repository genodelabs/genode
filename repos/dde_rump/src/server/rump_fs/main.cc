/**
 * \brief  RUMP file system interface implementation
 * \author Christian Helmuth
 * \author Sebastian Sumpf
 * \date   2014-01-14
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system/node_handle_registry.h>

#include "undef.h"

#include <file_system_session/rpc_object.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <root/component.h>
#include  <rump_fs/fs.h>
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
				Node *node = _handle_registry.lookup(packet.handle());

				_process_packet_op(packet, *node);
			}
			catch (Invalid_handle)     { PERR("Invalid_handle");     }
			catch (Size_limit_reached) { PERR("Size_limit_reached"); }

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
		 * Check if string represents a valid path (must start with '/')
		 */
		static void _assert_valid_path(char const *path)
		{
			if (!path || path[0] != '/') {
				PWRN("malformed path '%s'", path);
				throw Lookup_failed();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(size_t              tx_buf_size,
		                  Server::Entrypoint &ep,
		                  char const         *root_dir,
		                  bool                writeable,
		                  Allocator          &md_alloc)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_md_alloc(md_alloc),
			_root(*new (&_md_alloc) Directory(_md_alloc, root_dir, false)),
			_writable(writeable),
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

			if (!path.is_valid_string())
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

		void control(Node_handle, Control)
		{
			PERR("%s not implemented", __func__);
		}

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

		void move(Dir_handle, Name const &, Dir_handle, Name const &)
		{
			PERR("%s not implemented", __func__);
		}

		void sigh(Node_handle node_handle, Signal_context_capability sigh)
		{
			_handle_registry.sigh(node_handle, sigh);
		}

		void sync() { rump_sys_sync(); }
};

class File_system::Root : public Root_component<Session_component>
{
	private:

		Server::Entrypoint &_ep;

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

			try {
				Session_label  label(args);
				Session_policy policy(label);

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
				} catch (Xml_node::Nonexistent_attribute) {
					PERR("Missing \"root\" attribute in policy definition");
					throw Root::Unavailable();
				} catch (Lookup_failed) {
					PERR("Session root directory \"%s\" does not exist", root);
					throw Root::Unavailable();
				}

				/*
				 * Determine if write access is permitted for the session.
				 */
				try {
					writeable = policy.attribute("writeable").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) { }

			} catch (Session_policy::No_policy_defined) {
				PERR("Invalid session request, no matching policy");
				throw Root::Unavailable();
			}

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((size_t)4096, session_size) > ram_quota) {
				PERR("insufficient 'ram_quota', got %zd, need %zd",
				     ram_quota, session_size);
				throw Root::Quota_exceeded();
			}
			return new (md_alloc())
				Session_component(tx_buf_size, _ep, root_dir, writeable, *md_alloc());
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep          entrypoint
		 * \param sig_rec     signal receiver used for handling the
		 *                    data-flow signals of packet streams
		 * \param md_alloc    meta-data allocator
		 */
		Root(Server::Entrypoint &ep, Allocator &md_alloc)
		:
			Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep)
		{ }
};


struct File_system::Main
{
	Server::Entrypoint             &ep;
	Server::Signal_rpc_member<Main> resource_dispatcher;

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root = { ep, sliced_heap };


	/* return immediately from resource requests */
	void resource_handler(unsigned) { }

	Main(Server::Entrypoint &ep)
		:
			ep(ep),
			resource_dispatcher(ep, *this, &Main::resource_handler)
	{
		File_system::init(ep);
		env()->parent()->announce(ep.manage(fs_root));
		env()->parent()->resource_avail_sigh(resource_dispatcher);
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "rump_fs_ep"; }
Genode::size_t Server::stack_size()                      { return 4 * 1024 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static File_system::Main inst(ep); }

