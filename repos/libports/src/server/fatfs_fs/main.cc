/*
 * \brief  FATFS file system
 * \author Christian Prochaska
 * \date   2012-07-03
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/component.h>
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <base/attached_rom_dataspace.h>
#include <os/session_policy.h>
#include <util/xml_node.h>
#include <base/heap.h>
#include <base/log.h>

/* local includes */
#include <directory.h>
#include <file.h>
#include <open_node.h>
#include <util.h>

/* Genode block backend */
#include <fatfs/block.h>

/* fatfs includes */
namespace Fatfs { extern "C" {
#include <fatfs/ff.h>
} }


/*************************
 ** File-system service **
 *************************/

namespace Fatfs_fs {

	using namespace Genode;
	using File_system::Exception;
	using File_system::Packet_descriptor;
	using File_system::Path;

	class Session_component;
	class Root;
	struct Main;
}

class Fatfs_fs::Session_component : public Session_rpc_object
{
	private:

		typedef File_system::Open_node<Node> Open_node;

		Genode::Env          &_env;
		Genode::Allocator    &_heap;

		Directory                   &_root;
		Id_space<File_system::Node>  _open_node_registry;
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
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const offset  = packet.position();

			/* resulting length */
			size_t res_length = 0;
			bool succeeded = false;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				if (content && (packet.length() <= packet.size())) {
					res_length = open_node.node().read((char *)content, length,
					                                   offset);

					/* read data or EOF is a success */
					succeeded = res_length > 0;
					if (!succeeded) {
						File * file = dynamic_cast<File *>(&open_node.node());
						if (file)
							succeeded = f_eof((file->fatfs_fil()));
					}
				}
				break;

			case Packet_descriptor::WRITE:
				if (content && (packet.length() <= packet.size())) {
					res_length = open_node.node().write((char const *)content,
					                                    length, offset);

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

			case Packet_descriptor::CONTENT_CHANGED:
				open_node.register_notify(*tx_sink());
				open_node.node().notify_listeners();
				return;

			case Packet_descriptor::READ_READY:
				succeeded = true;
				/* not supported */
				break;

			case Packet_descriptor::SYNC:
				succeeded = true;
				/* not supported */
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
		 * Check if string represents a valid path (most start with '/')
		 */
		static void _assert_valid_path(char const *path)
		{
			if (!valid_path(path)) {
				warning("malformed path '", path, "'");
				throw Lookup_failed();
			}
		}

	public:

		/**
		 * Constructor
		 */
		Session_component(Genode::Env       &env,
			              Genode::Allocator &heap,
			              size_t             tx_buf_size,
			              Directory         &root,
			              bool               writable)
		:
			Session_rpc_object(env.ram().alloc(tx_buf_size),
				               env.rm(), env.ep().rpc_ep()),
			_env(env), _heap(heap), _root(root), _writable(writable),
			_process_packet_dispatcher(env.ep(), *this,
				                       &Session_component::_process_packets)
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
		}

		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name,
			             Mode mode, bool create)
		{
			if (!valid_filename(name.string()))
				throw Invalid_name();

			auto file_fn = [&] (Open_node &open_node) {

				using namespace Fatfs;

				FIL fatfs_fil;
				BYTE fatfs_flags = 0;

				if (!_writable)
					if (create || (mode != STAT_ONLY && mode != READ_ONLY))
						throw Permission_denied();

				if (create)
					fatfs_flags |= FA_CREATE_NEW;

				if ((mode == READ_ONLY) || (mode == READ_WRITE))
					fatfs_flags |= FA_READ;

				if ((mode == WRITE_ONLY) || (mode == READ_WRITE))
					fatfs_flags |= FA_WRITE;

				Absolute_path absolute_path(_root.name());

				try {
					absolute_path.append(open_node.node().name());
					absolute_path.append("/");
					absolute_path.append(name.string());
				} catch (Path_base::Path_too_long) {
					throw Invalid_name();
				}

				FRESULT res = f_open(&fatfs_fil, absolute_path.base(), fatfs_flags);

				switch(res) {
					case FR_OK: {
						File *file_node = new (&_heap) File(absolute_path.base());
						file_node->fatfs_fil(fatfs_fil);

						Open_node *open_file =
							new (_heap) Open_node(*file_node, _open_node_registry);

						return open_file->id();
					}
					case FR_NO_FILE:
					case FR_NO_PATH:
						throw Lookup_failed();
					case FR_INVALID_NAME:
					case FR_INVALID_DRIVE:
						throw Invalid_name();
					case FR_EXIST:
						throw Node_already_exists();
					case FR_DENIED:
					case FR_WRITE_PROTECTED:
						throw Permission_denied();
					case FR_NOT_READY:
						error("f_open() failed with error code FR_NOT_READY");
						throw Lookup_failed();
					case FR_DISK_ERR:
						error("f_open() failed with error code FR_DISK_ERR");
						throw Lookup_failed();
					case FR_INT_ERR:
						error("f_open() failed with error code FR_INT_ERR");
						throw Lookup_failed();
					case FR_NOT_ENABLED:
						error("f_open() failed with error code FR_NOT_ENABLED");
						throw Lookup_failed();
					case FR_NO_FILESYSTEM:
						error("f_open() failed with error code FR_NO_FILESYSTEM");
						throw Lookup_failed();
					default:
						/* not supposed to occur according to the fatfs documentation */
						error("f_open() returned an unexpected error code");
						throw Lookup_failed();
				}
			};

			try {
				return File_handle {
					_open_node_registry.apply<Open_node>(dir_handle, file_fn).value
				};
			} catch (Id_space<File_system::Node>::Unknown_id const &) {
				throw Invalid_handle();
			}
		}

		Symlink_handle symlink(Dir_handle, Name const &name, bool create)
		{
			/* not supported */
			throw Permission_denied();
		}

		Dir_handle dir(Path const &path, bool create)
		{
			if (create && !_writable)
				throw Permission_denied();

			_assert_valid_path(path.string());

			/*
			 *  The 'Directory' constructor removes trailing slashes,
			 *  except for "/"
			 */
			Directory *dir_node = new (&_heap) Directory(path.string());

			using namespace Fatfs;

			Absolute_path absolute_path(_root.name());

			try {
				absolute_path.append(dir_node->name());
				absolute_path.remove_trailing('/');
			} catch (Path_base::Path_too_long) {
				throw Name_too_long();
			}

			if (create) {

				if (is_root(dir_node->name()))
					throw Node_already_exists();

				FRESULT res = f_mkdir(absolute_path.base());

				try {
					switch(res) {
						case FR_OK:
							break;
						case FR_NO_PATH:
							throw Lookup_failed();
						case FR_INVALID_NAME:
							throw Name_too_long();
						case FR_INVALID_DRIVE:
							throw Name_too_long();
						case FR_DENIED:
						case FR_WRITE_PROTECTED:
							throw Permission_denied();
						case FR_EXIST:
							throw Node_already_exists();
						case FR_NOT_READY:
							error("f_mkdir() failed with error code FR_NOT_READY");
							throw Lookup_failed();
						case FR_DISK_ERR:
							error("f_mkdir() failed with error code FR_DISK_ERR");
							throw Lookup_failed();
						case FR_INT_ERR:
							error("f_mkdir() failed with error code FR_INT_ERR");
							throw Lookup_failed();
						case FR_NOT_ENABLED:
							error("f_mkdir() failed with error code FR_NOT_ENABLED");
							throw Lookup_failed();
						case FR_NO_FILESYSTEM:
							error("f_mkdir() failed with error code FR_NO_FILESYSTEM");
							throw Lookup_failed();
						default:
							/* not supposed to occur according to the fatfs documentation */
							error("f_mkdir() returned an unexpected error code");
							throw Lookup_failed();
					}
				} catch (Exception e) {
					destroy(&_heap, dir_node);
					throw e;
				}
			}

			Fatfs::DIR fatfs_dir;
			FRESULT f_opendir_res = f_opendir(&fatfs_dir, absolute_path.base());

			try {
				switch(f_opendir_res) {
					case FR_OK: {
						dir_node->fatfs_dir(fatfs_dir);

						Open_node *open_dir =
							new (_heap) Open_node(*dir_node, _open_node_registry);

						return Dir_handle { open_dir->id().value };
					}
					case FR_NO_PATH:
						throw Lookup_failed();
					case FR_INVALID_NAME:
						throw Name_too_long();
					case FR_INVALID_DRIVE:
						throw Name_too_long();
					case FR_NOT_READY:
						error("f_opendir() failed with error code FR_NOT_READY");
						throw Lookup_failed();
					case FR_DISK_ERR:
						error("f_opendir() failed with error code FR_DISK_ERR");
						throw Lookup_failed();
					case FR_INT_ERR:
						error("f_opendir() failed with error code FR_INT_ERR");
						throw Lookup_failed();
					case FR_NOT_ENABLED:
						error("f_opendir() failed with error code FR_NOT_ENABLED");
						throw Lookup_failed();
					case FR_NO_FILESYSTEM:
						error("f_opendir() failed with error code FR_NO_FILESYSTEM");
						throw Lookup_failed();
					default:
						/* not supposed to occur according to the fatfs documentation */
						error("f_opendir() returned an unexpected error code");
						throw Lookup_failed();
				}
			} catch (Exception e) {
				destroy(&_heap, dir_node);
				throw e;
			}
		}

		Node_handle node(Path const &path)
		{
			if (!valid_path(path.string()))
				throw Lookup_failed();

			Absolute_path absolute_path(_root.name());

			try {
				absolute_path.append(path.string());
				absolute_path.remove_trailing('/');
			} catch (Path_base::Path_too_long) {
				throw Lookup_failed();
			}

			Node *node = new (&_heap) Node(absolute_path.base());

			/* f_stat() does not work for "/" */
			if (!is_root(node->name())) {

				using namespace Fatfs;

				FILINFO file_info;

				FRESULT res = f_stat(node->name(), &file_info);

				try {
					switch(res) {
						case FR_OK:
							break;
						case FR_NO_FILE:
						case FR_NO_PATH:
							throw Lookup_failed();
						case FR_INVALID_NAME:
						case FR_INVALID_DRIVE:
							throw Lookup_failed();
						case FR_DISK_ERR:
							error("f_stat() failed with error code FR_DISK_ERR");
							throw Lookup_failed();
						case FR_INT_ERR:
							error("f_stat() failed with error code FR_INT_ERR");
							throw Lookup_failed();
						case FR_NOT_READY:
							error("f_stat() failed with error code FR_NOT_READY");
							throw Lookup_failed();
						case FR_NOT_ENABLED:
							error("f_stat() failed with error code FR_NOT_ENABLED");
							throw Lookup_failed();
						case FR_NO_FILESYSTEM:
							error("f_stat() failed with error code FR_NO_FILESYSTEM");
							throw Lookup_failed();
						default:
							/* not supposed to occur according to the fatfs documentation */
							error("f_stat() returned an unexpected error code");
							throw Lookup_failed();
					}
				} catch (Exception e) {
					destroy(&_heap, node);
					throw e;
				}
			}

			Open_node *open_node =
				new (_heap) Open_node(*node, _open_node_registry);

			return open_node->id();
		}

		void close(Node_handle handle)
		{
			auto close_fn = [&] (Open_node &open_node) {
				Node &node = open_node.node();
				destroy(_heap, &open_node);
				destroy(_heap, &node);
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

				Status status;
				status.inode = 1;
				status.size  = 0;
				status.mode  = 0;

				Node &node = open_node.node();

				using namespace Fatfs;

				/* f_stat() does not work for the '/' directory */
				if (!is_root(node.name())) {

					FILINFO fatfs_file_info;

					FRESULT res = f_stat(node.name(), &fatfs_file_info);

					switch(res) {
						case FR_OK:
							break;
						case FR_NO_FILE:
							error("f_stat() failed with error code FR_NO_FILE");
							return status;
						case FR_NO_PATH:
							error("f_stat() failed with error code FR_NO_PATH");
							return status;
						case FR_INVALID_NAME:
							error("f_stat() failed with error code FR_INVALID_NAME");
							return status;
						case FR_INVALID_DRIVE:
							error("f_stat() failed with error code FR_INVALID_DRIVE");
							return status;
						case FR_DISK_ERR:
							error("f_stat() failed with error code FR_DISK_ERR");
							return status;
						case FR_INT_ERR:
							error("f_stat() failed with error code FR_INT_ERR");
							return status;
						case FR_NOT_READY:
							error("f_stat() failed with error code FR_NOT_READY");
							return status;
						case FR_NOT_ENABLED:
							error("f_stat() failed with error code FR_NOT_ENABLED");
							return status;
						case FR_NO_FILESYSTEM:
							error("f_stat() failed with error code FR_NO_FILESYSTEM");
							return status;
						default:
							/* not supposed to occur according to the fatfs documentation */
							error("f_stat() returned an unexpected error code");
							return status;
					}

					if ((fatfs_file_info.fattrib & AM_DIR) == AM_DIR) {
						status.mode = File_system::Status::MODE_DIRECTORY; }
					else {
						status.mode = File_system::Status::MODE_FILE;
						status.size = fatfs_file_info.fsize;
					}

				} else {
					status.mode = File_system::Status::MODE_DIRECTORY;
				}

				if (status.mode == File_system::Status::MODE_DIRECTORY) {

					/* determine the number of directory entries */

					Fatfs::DIR fatfs_dir;
					FRESULT f_opendir_res = f_opendir(&fatfs_dir, node.name());

					if (f_opendir_res != FR_OK)
						return status;

					FILINFO fatfs_file_info;

					int num_direntries = -1;
					do {
						++num_direntries;
						FRESULT res = f_readdir(&fatfs_dir, &fatfs_file_info);
						if (res != FR_OK)
							return status;
					} while (fatfs_file_info.fname[0] != 0);

					status.size = num_direntries * sizeof(Directory_entry);
				}

				return status;
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
			if (!valid_filename(name.string()))
				throw Invalid_name();

			if (!_writable)
				throw Permission_denied();

			auto unlink_fn = [&] (Open_node &open_node) {

				using namespace Fatfs;

				Absolute_path absolute_path(_root.name());

				try {
					absolute_path.append(open_node.node().name());
					absolute_path.append("/");
					absolute_path.append(name.string());
				} catch (Path_base::Path_too_long) {
					throw Invalid_name();
				}

				FRESULT res = f_unlink(absolute_path.base());

				switch(res) {
					case FR_OK:
						break;
					case FR_NO_FILE:
					case FR_NO_PATH:
						throw Lookup_failed();
					case FR_INVALID_NAME:
					case FR_INVALID_DRIVE:
						throw Invalid_name();
					case FR_DENIED:
					case FR_WRITE_PROTECTED:
						throw Permission_denied();
					case FR_DISK_ERR:
						error("f_unlink() failed with error code FR_DISK_ERR");
						return;
					case FR_INT_ERR:
						error("f_unlink() failed with error code FR_INT_ERR");
						return;
					case FR_NOT_READY:
						error("f_unlink() failed with error code FR_NOT_READY");
						return;
					case FR_NOT_ENABLED:
						error("f_unlink() failed with error code FR_NOT_ENABLED");
						return;
					case FR_NO_FILESYSTEM:
						error("f_unlink() failed with error code FR_NO_FILESYSTEM");
						return;
					default:
						/* not supposed to occur according to the fatfs documentation */
						error("f_unlink() returned an unexpected error code");
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
			      Dir_handle to_dir_handle,   Name const &to_name)
		{
			if (!_writable)
				throw Permission_denied();

			if (!valid_filename(from_name.string()))
				throw Lookup_failed();

			if (!valid_filename(to_name.string()))
				throw Invalid_name();

			auto move_fn = [&] (Open_node &open_from_dir_node) {

				auto inner_move_fn = [&] (Open_node &open_to_dir_node) {

					Absolute_path absolute_from_path(_root.name());
					Absolute_path absolute_to_path(_root.name());

					try {
						absolute_from_path.append(open_from_dir_node.node().name());
						absolute_from_path.append("/");
						absolute_from_path.append(from_name.string());
						absolute_to_path.append(open_to_dir_node.node().name());
						absolute_to_path.append("/");
						absolute_to_path.append(to_name.string());
					} catch (Path_base::Path_too_long) {
						throw Invalid_name();
					}

					using namespace Fatfs;

					FRESULT res = f_rename(absolute_from_path.base(), absolute_to_path.base());

					/* if newpath already exists - try to unlink it once */
					if (res == FR_EXIST) {
						f_unlink(absolute_to_path.base());
						res = f_rename(absolute_from_path.base(), absolute_to_path.base());
					}

					switch(res) {
						case FR_OK:
							break;
						case FR_NO_FILE:
						case FR_NO_PATH:
							throw Lookup_failed();
						case FR_INVALID_NAME:
						case FR_INVALID_DRIVE:
							throw Invalid_name();
						case FR_EXIST:
							error("f_rename() failed with error code FR_EXIST");
							throw Invalid_name();
						case FR_DENIED:
						case FR_WRITE_PROTECTED:
							throw Permission_denied();
						case FR_DISK_ERR:
							error("f_rename() failed with error code FR_DISK_ERR");
							throw Lookup_failed();
						case FR_INT_ERR:
							error("f_rename() failed with error code FR_INT_ERR");
							throw Lookup_failed();
						case FR_NOT_READY:
							error("f_rename() failed with error code FR_NOT_READY");
							throw Lookup_failed();
						case FR_NOT_ENABLED:
							error("f_rename() failed with error code FR_NOT_ENABLED");
							throw Lookup_failed();
						case FR_NO_FILESYSTEM:
							error("f_rename() failed with error code FR_NO_FILESYSTEM");
							throw Lookup_failed();
						default:
							/* not supposed to occur according to the fatfs documentation */
							error("f_rename() returned an unexpected error code");
							throw Lookup_failed();
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

		void sigh(Node_handle, Genode::Signal_context_capability)
		{
			error("File_system::Session::sigh not supported");
		}
};


class Fatfs_fs::Root : public Root_component<Session_component>
{
	private:

		Genode::Env                   &_env;
		Genode::Allocator             &_md_alloc;
		Genode::Allocator             &_heap;
		Genode::Attached_rom_dataspace _config { _env, "config" };
		Directory                     &_root_dir;

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
				Session_policy policy(label, _config.xml());

				/*
				 * Determine directory that is used as root directory of
				 * the session.
				 */
				try {
					policy.attribute("root").value(root, sizeof(root));
					if (is_root(root)) {
						session_root_dir = &_root_dir;
					} else {
						/*
						 * Make sure the root path is specified with a
						 * leading path delimiter. For performing the
						 * lookup, we skip the first character.
						 */
						if (root[0] != '/')
							throw Lookup_failed();

						/* Check if the root path exists */

						using namespace Fatfs;

						FRESULT res = f_chdir(root);

						switch(res) {
							case FR_OK:
								break;
							case FR_NO_PATH:
								throw Lookup_failed();
							case FR_INVALID_NAME:
							case FR_INVALID_DRIVE:
								throw Lookup_failed();
							case FR_NOT_READY:
								error("f_chdir() failed with error code FR_NOT_READY");
								throw Service_denied();
							case FR_DISK_ERR:
								error("f_chdir() failed with error code FR_DISK_ERR");
								throw Service_denied();
							case FR_INT_ERR:
								error("f_chdir() failed with error code FR_INT_ERR");
								throw Service_denied();
							case FR_NOT_ENABLED:
								error("f_chdir() failed with error code FR_NOT_ENABLED");
								throw Service_denied();
							case FR_NO_FILESYSTEM:
								error("f_chdir() failed with error code FR_NO_FILESYSTEM");
								throw Service_denied();
							default:
								/* not supposed to occur according to the fatfs documentation */
								error("f_chdir() returned an unexpected error code");
								throw Service_denied();
						}

						session_root_dir = new (&_md_alloc) Directory(root);
					}
				}
				catch (Xml_node::Nonexistent_attribute) {
					error("missing \"root\" attribute in policy definition");
					throw Service_denied();
				}
				catch (Lookup_failed) {
					error("session root directory \"", Cstring(root), "\" does not exist");
					throw Service_denied();
				}

				/*
				 * Determine if write access is permitted for the session.
				 */
				writeable = policy.attribute_value("writeable", false);
			}
			catch (Session_policy::No_policy_defined) {
				error("Invalid session request, no matching policy");
				throw Service_denied();
			}

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			if (!tx_buf_size) {
				error(label, " requested a session with a zero length transmission buffer");
				throw Service_denied();
			}

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((size_t)4096, session_size) > ram_quota) {
				error("insufficient 'ram_quota', got ", ram_quota, ", "
					  "need ", session_size);
				throw Insufficient_ram_quota();
			}
			return new (md_alloc())
				Session_component(_env, _heap, tx_buf_size,
					              *session_root_dir, writeable);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param env   reference to Genode environment
		 * \param heap  meta-data allocator
		 * \param root  normal root directory if root in policy starts
		 *              at root
		 */
		Root(Genode::Env &env, Allocator &md_alloc, Genode::Allocator &heap,
			 Directory &root)
		:
			Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env), _md_alloc(md_alloc), _heap(heap), _root_dir(root)
		{ }
};


struct Fatfs_fs::Main
{
	Genode::Env         &_env;
	Genode::Heap         _heap        { _env.ram(), _env.rm() };
	Genode::Sliced_heap  _sliced_heap { _env.ram(), _env.rm() };

	Directory _root_dir { "/" };
	Root      _root { _env, _sliced_heap, _heap, _root_dir };

	Fatfs::FATFS _fatfs;

	Main(Genode::Env &env) : _env(env)
	{
		Fatfs::block_init(_env, _heap);

		using namespace File_system;
		using namespace Fatfs;

		/* mount the file system */
		if (f_mount(&_fatfs, "", 0) != Fatfs::FR_OK) {
			error("mount failed");

			struct Mount_failed : Genode::Exception { };
			throw Mount_failed();
		}

		Genode::log("--- Starting Fatfs_fs ---");

		_env.parent().announce(_env.ep().manage(_root));
	}
};


void Component::construct(Genode::Env &env)
{
	/* XXX execute constructors of global statics */
	env.exec_static_constructors();

	static Fatfs_fs::Main main(env);
}
