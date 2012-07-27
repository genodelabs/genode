/*
 * \brief  FFAT file system
 * \author Christian Prochaska
 * \date   2012-07-03
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system_session/rpc_object.h>
#include <root/component.h>
#include <cap_session/connection.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/session_policy.h>
#include <util/xml_node.h>

/* local includes */
#include <directory.h>
#include <file.h>
#include <node_handle_registry.h>
#include <util.h>

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

using namespace Genode;


static Lock _ffat_lock;
typedef Lock_guard<Lock> Ffat_lock_guard;


static bool const verbose = false;
#define PDBGV(...) if (verbose) PDBG(__VA_ARGS__)


/*************************************
 ** Helpers for dispatching signals **
 *************************************/

namespace Genode {

	struct Signal_dispatcher_base : Signal_context
	{
		virtual void dispatch(int num) = 0;
	};


	template <typename T>
	class Signal_dispatcher : private Signal_dispatcher_base,
	                          public  Signal_context_capability
	{
		private:

			T &obj;
			void (T::*member) (int);
			Signal_receiver &sig_rec;

		public:

			/**
			 * Constructor
			 *
			 * \param sig_rec     signal receiver to associate the signal
			 *                    handler with
			 * \param obj,member  object and member function to call when
			 *                    the signal occurs
			 */
			Signal_dispatcher(Signal_receiver &sig_rec,
			                  T &obj, void (T::*member)(int))
			:
				Signal_context_capability(sig_rec.manage(this)),
				obj(obj), member(member),
				sig_rec(sig_rec)
			{ }

			~Signal_dispatcher() { sig_rec.dissolve(this); }

			void dispatch(int num) { (obj.*member)(num); }
	};
}


/*************************
 ** File-system service **
 *************************/

namespace File_system {

	class Session_component : public Session_rpc_object
	{
		private:

			Directory            &_root;
			Node_handle_registry  _handle_registry;
			bool                  _writable;

			Signal_dispatcher<Session_component> _process_packet_dispatcher;


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

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				switch (packet.operation()) {

					case Packet_descriptor::READ:
						PDBGV("READ");
						res_length = node.read((char *)content, length, offset);
						break;

					case Packet_descriptor::WRITE:
						PDBGV("WRITE");
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
			void _process_packets(int)
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
					PWRN("malformed path '%s'", path);
					throw Lookup_failed();
				}
			}

			/**
			 * Return true if both '_root.name()' and 'path'
			 * are "/"
			 */
			bool is_ffat_root(const char *path)
			{
				return (is_root(_root.name()) && is_root(path));
			}

		public:

			/**
			 * Constructor
			 */
			Session_component(size_t tx_buf_size, Rpc_entrypoint &ep,
			                  Signal_receiver &sig_rec,
			                  Directory &root, bool writable)
			:
				Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep),
				_root(root),
				_writable(writable),
				_process_packet_dispatcher(sig_rec, *this,
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
				env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
			}


			/***************************
			 ** File_system interface **
			 ***************************/

			File_handle file(Dir_handle dir_handle, Name const &name,
			                 Mode mode, bool create)
			{
				PDBGV("_root = %s, dir_name = %s, name = %s, create = %d",
 				      _root.name(),
				      _handle_registry.lookup(dir_handle)->name(),
				      name.string(),
				      create);

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				if (!valid_filename(name.string()))
					throw Invalid_name();

				using namespace Ffat;

				FIL ffat_fil;
				BYTE ffat_flags = 0;

				if (!_writable)
					if (create || (mode != STAT_ONLY && mode != READ_ONLY))
						throw Permission_denied();

				if (create)
					ffat_flags |= FA_CREATE_ALWAYS; /* overwrite existing file */

				if ((mode == READ_ONLY) || (mode == READ_WRITE))
					ffat_flags |= FA_READ;

				if ((mode == WRITE_ONLY) || (mode == READ_WRITE))
					ffat_flags |= FA_WRITE;

				f_chdir(_root.name());
				f_chdir(&_handle_registry.lookup(dir_handle)->name()[1]);

				FRESULT res = f_open(&ffat_fil, name.string(), ffat_flags);

				switch(res) {
					case FR_OK: {
						File *file_node = new (env()->heap()) File(name.string());
						file_node->ffat_fil(ffat_fil);
						return _handle_registry.alloc(file_node);
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
						PERR("f_open() failed with error code FR_NOT_READY");
						throw Lookup_failed();
					case FR_DISK_ERR:
						PERR("f_open() failed with error code FR_DISK_ERR");
						throw Lookup_failed();
					case FR_INT_ERR:
						PERR("f_open() failed with error code FR_INT_ERR");
						throw Lookup_failed();
					case FR_NOT_ENABLED:
						PERR("f_open() failed with error code FR_NOT_ENABLED");
						throw Lookup_failed();
					case FR_NO_FILESYSTEM:
						PERR("f_open() failed with error code FR_NO_FILESYSTEM");
						throw Lookup_failed();
					default:
						/* not supposed to occur according to the libffat documentation */
						PERR("f_open() returned an unexpected error code");
						throw Lookup_failed();
				}
			}

			Symlink_handle symlink(Dir_handle, Name const &name, bool create)
			{
				/* not supported */
				return Symlink_handle(-1);
			}

			Dir_handle dir(Path const &path, bool create)
			{
				PDBGV("_root = %s, path = %s, create = %d",
					  _root.name(), path.string(), create);

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				if (create && !_writable)
					throw Permission_denied();

				_assert_valid_path(path.string());

				/*
				 *  The 'Directory' constructor removes trailing slashes,
				 *  except for "/"
				 */
				Directory *dir_node = new (env()->heap()) Directory(path.string());

				using namespace Ffat;

				f_chdir(_root.name());

				if (create) {

					if (is_root(dir_node->name()))
						throw Node_already_exists();

					FRESULT res = f_mkdir(&dir_node->name()[1]);

					try {
						switch(res) {
							case FR_OK:
								break;
							case FR_NO_PATH:
								PDBGV("f_mkdir() failed with error code FR_NO_PATH");
								throw Lookup_failed();
							case FR_INVALID_NAME:
								PDBGV("f_mkdir() failed with error code FR_INVALID_NAME");
								throw Name_too_long();
							case FR_INVALID_DRIVE:
								PDBGV("f_mkdir() failed with error code FR_INVALID_DRIVE");
								throw Name_too_long();
							case FR_DENIED:
							case FR_WRITE_PROTECTED:
								throw Permission_denied();
							case FR_EXIST:
								throw Node_already_exists();
							case FR_NOT_READY:
								PERR("f_mkdir() failed with error code FR_NOT_READY");
								throw Lookup_failed();
							case FR_DISK_ERR:
								PERR("f_mkdir() failed with error code FR_DISK_ERR");
								throw Lookup_failed();
							case FR_INT_ERR:
								PERR("f_mkdir() failed with error code FR_INT_ERR");
								throw Lookup_failed();
							case FR_NOT_ENABLED:
								PERR("f_mkdir() failed with error code FR_NOT_ENABLED");
								throw Lookup_failed();
							case FR_NO_FILESYSTEM:
								PERR("f_mkdir() failed with error code FR_NO_FILESYSTEM");
								throw Lookup_failed();
							default:
								/* not supposed to occur according to the libffat documentation */
								PERR("f_mkdir() returned an unexpected error code");
								throw Lookup_failed();
						}
					} catch (Exception e) {
						PDBGV("exception occured while trying to create directory");
						destroy(env()->heap(), dir_node);
						throw e;
					}
				}

				Ffat::DIR ffat_dir;
				FRESULT f_opendir_res = f_opendir(&ffat_dir, &dir_node->name()[1]);

				try {
					switch(f_opendir_res) {
						case FR_OK:
							dir_node->ffat_dir(ffat_dir);
							return _handle_registry.alloc(dir_node);
						case FR_NO_PATH:
							PDBGV("f_opendir() failed with error code FR_NO_PATH");
							throw Lookup_failed();
						case FR_INVALID_NAME:
							PDBGV("f_opendir() failed with error code FR_INVALID_NAME");
							throw Name_too_long();
						case FR_INVALID_DRIVE:
							PDBGV("f_opendir() failed with error code FR_INVALID_DRIVE");
							throw Name_too_long();
						case FR_NOT_READY:
							PERR("f_opendir() failed with error code FR_NOT_READY");
							throw Lookup_failed();
						case FR_DISK_ERR:
							PERR("f_opendir() failed with error code FR_DISK_ERR");
							throw Lookup_failed();
						case FR_INT_ERR:
							PERR("f_opendir() failed with error code FR_INT_ERR");
							throw Lookup_failed();
						case FR_NOT_ENABLED:
							PERR("f_opendir() failed with error code FR_NOT_ENABLED");
							throw Lookup_failed();
						case FR_NO_FILESYSTEM:
							PERR("f_opendir() failed with error code FR_NO_FILESYSTEM");
							throw Lookup_failed();
						default:
							/* not supposed to occur according to the libffat documentation */
							PERR("f_opendir() returned an unexpected error code");
							throw Lookup_failed();
					}
				} catch (Exception e) {
					destroy(env()->heap(), dir_node);
					throw e;
				}
			}

			Node_handle node(Path const &path)
			{
				PDBGV("path = %s", path.string());

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				if (!valid_path(path.string()) &&
				    !valid_filename(path.string()))
					throw Lookup_failed();

				/*
				 * The Node constructor removes trailing slashes,
				 * except for "/"
				 */
				Node *node = new (env()->heap()) Node(path.string());

				/* f_stat() does not work for "/" */
				if (!is_ffat_root(node->name())) {

					using namespace Ffat;

					FILINFO file_info;
					/* the long file name is not used in this function */
					file_info.lfname = 0;
					file_info.lfsize = 0;

					FRESULT res;

					/*
					 * f_stat() does not work on an empty relative name,
					 * so in this case the absolute root name is used
					 */
					if (!is_root(node->name())) {
						f_chdir(_root.name());
						res = f_stat(&node->name()[1], &file_info);
					} else
						res = f_stat(_root.name(), &file_info);

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
								PERR("f_stat() failed with error code FR_DISK_ERR");
								throw Lookup_failed();
							case FR_INT_ERR:
								PERR("f_stat() failed with error code FR_INT_ERR");
								throw Lookup_failed();
							case FR_NOT_READY:
								PERR("f_stat() failed with error code FR_NOT_READY");
								throw Lookup_failed();
							case FR_NOT_ENABLED:
								PERR("f_stat() failed with error code FR_NOT_ENABLED");
								throw Lookup_failed();
							case FR_NO_FILESYSTEM:
								PERR("f_stat() failed with error code FR_NO_FILESYSTEM");
								throw Lookup_failed();
							default:
								/* not supposed to occur according to the libffat documentation */
								PERR("f_stat() returned an unexpected error code");
								throw Lookup_failed();
						}
					} catch (Exception e) {
						destroy(env()->heap(), node);
						throw e;
					}
				}

				return _handle_registry.alloc(node);
			}

			void close(Node_handle handle)
			{
				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				Node *node;

				try {
					node = _handle_registry.lookup(handle);
				} catch(Invalid_handle) {
					PERR("close() called with invalid handle");
					return;
				}

				PDBGV("name = %s", node->name());

				/* free the handle */
				_handle_registry.free(handle);

				File *file = dynamic_cast<File *>(node);
				if (file) {
					using namespace Ffat;

					FRESULT res = f_close(file->ffat_fil());

					/* free the node */
					destroy(env()->heap(), file);

					switch(res) {
						case FR_OK:
							return;
						case FR_INVALID_OBJECT:
							PERR("f_close() failed with error code FR_INVALID_OBJECT");
							return;
						case FR_DISK_ERR:
							PERR("f_close() failed with error code FR_DISK_ERR");
							return;
						case FR_INT_ERR:
							PERR("f_close() failed with error code FR_INT_ERR");
							return;
						case FR_NOT_READY:
							PERR("f_close() failed with error code FR_NOT_READY");
							return;
						default:
							/* not supposed to occur according to the libffat documentation */
							PERR("f_close() returned an unexpected error code");
							return;
					}
				}
			}

			Status status(Node_handle node_handle)
			{
				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				Status status;
				status.inode = 1;
				status.size  = 0;
				status.mode  = 0;

				Node *node = _handle_registry.lookup(node_handle);

				PDBGV("name = %s", node->name());

				using namespace Ffat;

				const char *ffat_name;

				/*
				 * f_stat() does not work on an empty relative name,
				 * so in this case the absolute root name is used
				 */
				if (!is_root(node->name())) {
					f_chdir(_root.name());
					if (node->name()[0] == '/')
						ffat_name = &node->name()[1];
					else
						ffat_name = node->name();
				} else
					ffat_name = _root.name();

				/* f_stat() does not work for the '/' directory */
				if (!is_ffat_root(node->name())) {

					FILINFO ffat_file_info;
					/* the long file name is not used in this function */
					ffat_file_info.lfname = 0;
					ffat_file_info.lfsize = 0;

					FRESULT res = f_stat(ffat_name, &ffat_file_info);

					switch(res) {
						case FR_OK:
							break;
						case FR_NO_FILE:
							PERR("f_stat() failed with error code FR_NO_FILE");
							return status;
						case FR_NO_PATH:
							PERR("f_stat() failed with error code FR_NO_PATH");
							return status;
						case FR_INVALID_NAME:
							PERR("f_stat() failed with error code FR_INVALID_NAME");
							return status;
						case FR_INVALID_DRIVE:
							PERR("f_stat() failed with error code FR_INVALID_DRIVE");
							return status;
						case FR_DISK_ERR:
							PERR("f_stat() failed with error code FR_DISK_ERR");
							return status;
						case FR_INT_ERR:
							PERR("f_stat() failed with error code FR_INT_ERR");
							return status;
						case FR_NOT_READY:
							PERR("f_stat() failed with error code FR_NOT_READY");
							return status;
						case FR_NOT_ENABLED:
							PERR("f_stat() failed with error code FR_NOT_ENABLED");
							return status;
						case FR_NO_FILESYSTEM:
							PERR("f_stat() failed with error code FR_NO_FILESYSTEM");
							return status;
						default:
							/* not supposed to occur according to the libffat documentation */
							PERR("f_stat() returned an unexpected error code");
							return status;
					}

					if ((ffat_file_info.fattrib & AM_DIR) == AM_DIR) {
						PDBGV("MODE_DIRECTORY");
						status.mode = File_system::Status::MODE_DIRECTORY; }
					else {
						PDBGV("MODE_FILE");
						status.mode = File_system::Status::MODE_FILE;
						status.size = ffat_file_info.fsize;
					}

				} else {
					PDBGV("MODE_DIRECTORY");
					status.mode = File_system::Status::MODE_DIRECTORY;
				}

				if (status.mode == File_system::Status::MODE_DIRECTORY) {

					/* determine the number of directory entries */

					Ffat::DIR ffat_dir;
					FRESULT f_opendir_res = f_opendir(&ffat_dir, ffat_name);

					if (f_opendir_res != FR_OK)
						return status;

					FILINFO ffat_file_info;
					/* the long file name is not used in this function */
					ffat_file_info.lfname = 0;
					ffat_file_info.lfsize = 0;

					int num_direntries = -1;
					do {
						++num_direntries;
						FRESULT res = f_readdir(&ffat_dir, &ffat_file_info);
						if (res != FR_OK)
							return status;
					} while (ffat_file_info.fname[0] != 0);

					status.size = num_direntries * sizeof(Directory_entry);
				}

				return status;
			}

			void control(Node_handle, Control) { }

			void unlink(Dir_handle dir_handle, Name const &name)
			{
				PDBGV("name = %s", name.string());

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				if (!valid_filename(name.string()))
					throw Invalid_name();

				if (!_writable)
					throw Permission_denied();

				using namespace Ffat;

				f_chdir(_root.name());

				f_chdir(&_handle_registry.lookup(dir_handle)->name()[1]);

				FRESULT res = f_unlink(name.string());

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
						PERR("f_unlink() failed with error code FR_DISK_ERR");
						return;
					case FR_INT_ERR:
						PERR("f_unlink() failed with error code FR_INT_ERR");
						return;
					case FR_NOT_READY:
						PERR("f_unlink() failed with error code FR_NOT_READY");
						return;
					case FR_NOT_ENABLED:
						PERR("f_unlink() failed with error code FR_NOT_ENABLED");
						return;
					case FR_NO_FILESYSTEM:
						PERR("f_unlink() failed with error code FR_NO_FILESYSTEM");
						return;
					default:
						/* not supposed to occur according to the libffat documentation */
						PERR("f_unlink() returned an unexpected error code");
						return;
				}
			}

			void truncate(File_handle file_handle, file_size_t size)
			{
				PDBGV("truncate()");

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				if (!_writable)
					throw Permission_denied();

				File *file = _handle_registry.lookup(file_handle);

				using namespace Ffat;

				FRESULT res = f_truncate(file->ffat_fil());

				switch(res) {
					case FR_OK:
						return;
					case FR_INVALID_OBJECT:
						PERR("f_truncate() failed with error code FR_INVALID_OBJECT");
						return;
					case FR_DISK_ERR:
						PERR("f_truncate() failed with error code FR_DISK_ERR");
						return;
					case FR_INT_ERR:
						PERR("f_truncate() failed with error code FR_INT_ERR");
						return;
					case FR_NOT_READY:
						PERR("f_truncate() failed with error code FR_NOT_READY");
						return;
					case FR_TIMEOUT:
						PERR("f_truncate() failed with error code FR_TIMEOUT");
						return;
					default:
						/* not supposed to occur according to the libffat documentation */
						PERR("f_truncate() returned an unexpected error code");
						return;
				}
			}

			void move(Dir_handle from_dir_handle, Name const &from_name,
			          Dir_handle to_dir_handle,   Name const &to_name)
			{
				PDBGV("from_name = %s, to_name = %s", from_name.string(), to_name.string());

				Ffat_lock_guard ffat_lock_guard(_ffat_lock);

				if (!_writable)
					throw Permission_denied();

				if (!valid_filename(from_name.string()))
					throw Lookup_failed();

				if (!valid_filename(to_name.string()))
					throw Invalid_name();

				Directory *from_dir = _handle_registry.lookup(from_dir_handle);
				Directory *to_dir = _handle_registry.lookup(to_dir_handle);

				using namespace Ffat;

				f_chdir(_root.name());

				char from_path[2*(_MAX_LFN + 1)];
				char to_path[2*(_MAX_LFN + 1)];

				strncpy(from_path, &from_dir->name()[1], _MAX_LFN);
				strncpy(&from_path[strlen(from_path)], "/", 2);
				strncpy(&from_path[strlen(from_path)], from_name.string(), _MAX_LFN + 1);

				strncpy(to_path, &to_dir->name()[1], _MAX_LFN);
				strncpy(&to_path[strlen(to_path)], "/", 2);
				strncpy(&to_path[strlen(to_path)], to_name.string(), _MAX_LFN + 1);

				PDBGV("from_path = %s", from_path);
				PDBGV("to_path = %s", to_path);

				FRESULT res = f_rename(from_path, to_path);

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
						PERR("f_rename() failed with error code FR_EXIST");
						throw Invalid_name();
					case FR_DENIED:
					case FR_WRITE_PROTECTED:
						throw Permission_denied();
					case FR_DISK_ERR:
						PERR("f_rename() failed with error code FR_DISK_ERR");
						throw Lookup_failed();
					case FR_INT_ERR:
						PERR("f_rename() failed with error code FR_INT_ERR");
						throw Lookup_failed();
					case FR_NOT_READY:
						PERR("f_rename() failed with error code FR_NOT_READY");
						throw Lookup_failed();
					case FR_NOT_ENABLED:
						PERR("f_rename() failed with error code FR_NOT_ENABLED");
						throw Lookup_failed();
					case FR_NO_FILESYSTEM:
						PERR("f_rename() failed with error code FR_NO_FILESYSTEM");
						throw Lookup_failed();
					default:
						/* not supposed to occur according to the libffat documentation */
						PERR("f_rename() returned an unexpected error code");
						throw Lookup_failed();
				}

			}
	};


	class Root : public Root_component<Session_component>
	{
		private:

			Rpc_entrypoint  &_channel_ep;
			Signal_receiver &_sig_rec;
			Directory       &_root_dir;

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

				try {
					Session_policy policy(args);

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

							using namespace Ffat;

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
									PERR("f_chdir() failed with error code FR_NOT_READY");
									throw Root::Unavailable();
								case FR_DISK_ERR:
									PERR("f_chdir() failed with error code FR_DISK_ERR");
									throw Root::Unavailable();
								case FR_INT_ERR:
									PERR("f_chdir() failed with error code FR_INT_ERR");
									throw Root::Unavailable();
								case FR_NOT_ENABLED:
									PERR("f_chdir() failed with error code FR_NOT_ENABLED");
									throw Root::Unavailable();
								case FR_NO_FILESYSTEM:
									PERR("f_chdir() failed with error code FR_NO_FILESYSTEM");
									throw Root::Unavailable();
								default:
									/* not supposed to occur according to the libffat documentation */
									PERR("f_chdir() returned an unexpected error code");
									throw Root::Unavailable();
							}

							session_root_dir = new (env()->heap()) Directory(root);
						}
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
					Session_component(tx_buf_size, _channel_ep, _sig_rec,
					                  *session_root_dir, writeable);
			}

		public:

			/**
			 * Constructor
			 *
			 * \param session_ep  session entrypoint
			 * \param sig_rec     signal receiver used for handling the
			 *                    data-flow signals of packet streams
			 * \param md_alloc    meta-data allocator
			 */
			Root(Rpc_entrypoint &session_ep, Allocator &md_alloc,
			     Signal_receiver &sig_rec, Directory &root_dir)
			:
				Root_component<Session_component>(&session_ep, &md_alloc),
				_channel_ep(session_ep), _sig_rec(sig_rec), _root_dir(root_dir)
			{ }
	};
};


int main(int, char **)
{
	using namespace File_system;
	using namespace Ffat;

	static Ffat::FATFS _fatfs;

	/* mount the file system */
	PDBGV("Mounting device %u ...\n\n", 0);

	if (f_mount(0, &_fatfs) != Ffat::FR_OK) {
		PERR("Mount failed\n");
		return -1;
	}

	enum { STACK_SIZE = 3*sizeof(addr_t)*1024 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "ffat_fs_ep");
	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());
	static Signal_receiver sig_rec;

	static Directory root_dir("/");

	static File_system::Root root(ep, sliced_heap, sig_rec, root_dir);

	env()->parent()->announce(ep.manage(&root));

	for (;;) {
		Signal s = sig_rec.wait_for_signal();
		static_cast<Signal_dispatcher_base *>(s.context())->dispatch(s.num());
	}

	return 0;
}
