/*
 * \brief  FUSE file system
 * \author Josef Soentgen
 * \date   2013-11-27
 */

/*
 * Copyright (C) 2013-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <file_system/node_handle_registry.h>
#include <file_system_session/rpc_object.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <util/xml_node.h>

/* libc includes */
#include <errno.h>

/* local includes */
#include <directory.h>
#include <util.h>


namespace File_system {
	struct Main;
	struct Session_component;
	struct Root;
}


class File_system::Session_component : public Session_rpc_object
{
	private:

		Server::Entrypoint   &_ep;
		Allocator            &_md_alloc;
		Directory            &_root;
		Node_handle_registry  _handle_registry;
		bool                  _writeable;

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
				/* session is read-only */
				if (!_writeable)
					break;

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
		Session_component(size_t              tx_buf_size,
		                  Server::Entrypoint &ep,
		                  char const         *root_dir,
		                  bool                writeable,
		                  Allocator          &md_alloc)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_ep(ep),
			_md_alloc(md_alloc),
			_root(*new (&_md_alloc) Directory(_md_alloc, root_dir, false)),
			_writeable(writeable),
			_process_packet_dispatcher(_ep, *this, &Session_component::_process_packets)
		{
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			Fuse::sync_fs();

			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
			destroy(&_md_alloc, &_root);
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_filename(name.string()))
				throw Invalid_name();

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (create && !_writeable)
				throw Permission_denied();

			File *file = new (&_md_alloc) File(dir, name.string(), mode, create);
			Node_lock_guard file_guard(file);

			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{
			if (! Fuse::support_symlinks()) {
				Genode::error("FUSE file system does not support symlinks");
				return Symlink_handle();
			}

			if (!valid_filename(name.string()))
			    throw Invalid_name();

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			if (create && !_writeable)
					throw Permission_denied();

			Symlink *symlink = new (&_md_alloc) Symlink(dir, name.string(), create);
			Node_lock_guard symlink_guard(symlink);

			return _handle_registry.alloc(symlink);
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

			Node_lock_guard guard(dir_node);
			return _handle_registry.alloc(dir_node);
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

			Node_lock_guard guard(node);
			return _handle_registry.alloc(node);
		}

		void close(Node_handle handle)
		{
			Node *node;

			try {
				node = _handle_registry.lookup_and_lock(handle);
				/**
				 * We need to call unlock() here because the handle registry
				 * calls lock() itself on the node.
				 */
				node->unlock();
			} catch (Invalid_handle) {
				Genode::error("close() called with invalid handle");
				return;
			}

			_handle_registry.free(handle);
			destroy(&_md_alloc, node);
		}

		Status status(Node_handle node_handle)
		{
			Node *node = _handle_registry.lookup_and_lock(node_handle);
			Node_lock_guard guard(node);

			File *file = dynamic_cast<File *>(node);
			if (file)
				return file->status();

			Directory *dir = dynamic_cast<Directory *>(node);
			if (dir)
				return dir->status();

			Symlink *symlink = dynamic_cast<Symlink *>(node);
			if (symlink)
				return symlink->status();

			return Status();
		}

		void control(Node_handle, Control)
		{
			Genode::error(__func__, " not implemented");
		}

		void unlink(Dir_handle dir_handle, Name const &name)
		{
			if (!_writeable)
				throw Permission_denied();

			Directory *dir = _handle_registry.lookup_and_lock(dir_handle);
			Node_lock_guard dir_guard(dir);

			Absolute_path absolute_path(_root.name());

			try {
				absolute_path.append(dir->name());
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
		}

		void truncate(File_handle file_handle, file_size_t size)
		{
			if (!_writeable)
				throw Permission_denied();

			File *file;
			try { file = _handle_registry.lookup_and_lock(file_handle); }
			catch (Invalid_handle) { throw Lookup_failed(); }

			Node_lock_guard file_guard(file);
			file->truncate(size);
		}

		void move(Dir_handle from_dir_handle, Name const &from_name,
			  Dir_handle to_dir_handle,   Name const &to_name)
		{
			if (!_writeable)
				throw Permission_denied();
			
			Directory *from_dir, *to_dir;
			try { from_dir = _handle_registry.lookup_and_lock(from_dir_handle); }
			catch (Invalid_handle) { throw Lookup_failed(); }

			try { to_dir = _handle_registry.lookup_and_lock(to_dir_handle); }
			catch (Invalid_handle) {
				from_dir->unlock();
				throw Lookup_failed();
			}

			Node_lock_guard from_dir_guard(from_dir);
			Node_lock_guard to_dir_guard(to_dir);

			Absolute_path absolute_from_path(_root.name());
			Absolute_path absolute_to_path(_root.name());

			try {
				absolute_from_path.append(from_dir->name());
				absolute_from_path.append("/");
				absolute_from_path.append(from_name.string());
				absolute_to_path.append(to_dir->name());
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
		}

		void sigh(Node_handle node_handle, Signal_context_capability sigh)
		{
			_handle_registry.sigh(node_handle, sigh);
		}

		void sync(Node_handle) override
		{
			Fuse::sync_fs();
		}
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

			Session_label const label = label_from_args(args);
			try {
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
				if (writeable)
					Genode::warning("write support in fuse_fs is considered experimental, data-loss may occur.");

			} catch (Session_policy::No_policy_defined) {
				Genode::error("Invalid session request, no matching policy");
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
				Genode::error("insufficient 'ram_quota', got ", ram_quota, " , "
				              "need ", session_size);
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
	Server::Entrypoint &ep;

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root = { ep, sliced_heap };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		if (!Fuse::init_fs()) {
			Genode::error("FUSE fs initialization failed");
			return;
		}

		env()->parent()->announce(ep.manage(fs_root));
	}

	~Main()
	{
		if (Fuse::initialized()) {
			Fuse::deinit_fs();
		}
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "fuse_fs_ep"; }
/**
 * The large stack is needed because FUSE file system may call
 * libc functions that require a large stack, e.g. timezone
 * related functions.
 */
Genode::size_t Server::stack_size()                      { return 8192 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static File_system::Main inst(ep); }
