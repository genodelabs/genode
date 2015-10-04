/*
 * \brief  Server that writes log messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <root/component.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <base/printf.h>

/* Local includes */
#include "log_file.h"
#include "session.h"

namespace Fs_log {

	using namespace Genode;
	using namespace File_system;

	class  Root_component;
	struct Main;

	enum {
		 BLOCK_SIZE = Log_session::String::MAX_SIZE,
		 QUEUE_SIZE = File_system::Session::TX_QUEUE_SIZE,
		TX_BUF_SIZE = BLOCK_SIZE * (QUEUE_SIZE*2 + 1)
	};

}

class Fs_log::Root_component :
	public Genode::Root_component<Fs_log::Session_component>
{
	private:

		Allocator_avl            _write_alloc;
		File_system::Connection  _fs;
		List<Log_file>           _log_files;

		Log_file *lookup(char const *dir, char const *filename)
		{
			for (Log_file *file = _log_files.first(); file; file = file->next())
				if (file->match(dir, filename))
					return file;

			return 0;
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace File_system;

			char  dir_path[MAX_PATH_LEN];
			char file_name[MAX_NAME_LEN];

			dir_path[0] = '/';

			bool truncate = false;
			Session_label session_label(args);
			char const *label_str = session_label.string();
			char const *label_prefix = nullptr;

			try {
				Session_policy policy(session_label);
				try {
					truncate = policy.attribute("truncate").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) { }

				bool merge = false;
				try {
					merge = policy.attribute("merge").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) { }
				if (merge) {
					/*
					 * Use the policy label that was matched rather than
					 * full session label for the path of the log file.
					 */
					policy.attribute("label").value(dir_path+1, sizeof(dir_path)-1);
					if (!dir_path[1]) {
						PERR("cannot merge an empty policy label");
						throw Root::Unavailable();
					}

					/*
					 * If the policy has a trailing '->', move first element
					 * from the log prefix to the end of the log path.
					 */
					size_t label_len = strlen(dir_path);
					label_prefix = label_str+(label_len-1);

					if ((strcmp((dir_path+label_len)-3, " ->", 4) == 0) ||
					    (strcmp((dir_path+label_len)-4, " -> ", 5) == 0)) {

						for (size_t i = 0;; ++i) {
							if (label_prefix[i] == '\0') {
								strncpy(dir_path+1, label_str, MAX_PATH_LEN-1);
								label_prefix = nullptr;
								break;
							}

							if (strcmp(label_prefix+i, " -> ", 4))
								continue;

							strncpy(dir_path+label_len, label_prefix, i+1);
							label_prefix += i+4;
							break;
						}
					}

				} else
					strncpy(dir_path+1, label_str, MAX_PATH_LEN-1);

			} catch (Session_policy::No_policy_defined) {
				strncpy(dir_path+1, label_str, MAX_PATH_LEN-1);
			}

			{
				/* Parse out a directory and file name. */
				size_t len = strlen(dir_path);
				size_t start = 1;
				for (size_t i = 1; i < len;) {
					/* Replace any slashes in label elements. */
					if (dir_path[i] == '/') dir_path[i] = '_';
					if (strcmp(" -> ", dir_path+i, 4) == 0) {
						dir_path[i++] = '/';
						strncpy(dir_path+i, dir_path+i+3, MAX_PATH_LEN-i);
						start = i;
						i += 3;
					} else ++i;
				}

				/* Copy the remainder to the file name. */
				snprintf(file_name, MAX_NAME_LEN, "%s.log", dir_path+start);

				/* Terminate the directory path. */
				dir_path[(start == 1) ? start : start-1] = '\0';

				/* Rewrite any slashes in the name. */
				for (char *p = file_name; *p; ++p)
					if (*p == '/') *p = '_';
			}

			Log_file *file = lookup(dir_path, file_name);
			if (!file) try {

				Dir_handle   dir_handle = ensure_dir(_fs, dir_path);
				Handle_guard dir_guard(_fs, dir_handle);
				File_handle  handle;
				seek_off_t   offset = 0;

				try {
					handle = _fs.file(dir_handle, file_name,
					                  File_system::WRITE_ONLY, false);

					if (truncate)
						_fs.truncate(handle, 0);
					else
						offset = _fs.status(handle).size;

				} catch (File_system::Lookup_failed) {
					handle = _fs.file(dir_handle, file_name,
					                  File_system::WRITE_ONLY, true);
				}

				file = new (env()->heap())
					Log_file(_fs, handle, dir_path, file_name, offset);

				_log_files.insert(file);

			} catch (Permission_denied) {
				PERR("%s:%s: permission denied", dir_path, file_name);

			} catch (Name_too_long) {
				PERR("%s:%s: name too long", dir_path, file_name);

			} catch (No_space) {
				PERR("%s:%s: no space", dir_path, file_name);

			} catch (Out_of_node_handles) {
				PERR("%s:%s: out of node handles", dir_path, file_name);

			} catch (Invalid_name) {
				PERR("%s:%s: invalid_name", dir_path, file_name);

			} catch (Size_limit_reached) {
				PERR("%s:%s: size limit reached", dir_path, file_name);

			} catch (...) {
				PERR("%s:%s: unknown error", dir_path, file_name);
				throw;
			}
			if (!file) {
				PERR("file was null");
				throw Root::Unavailable();
			}

			if (label_prefix && *label_prefix)
				return new (md_alloc()) Labeled_session_component(label_prefix, *file);
			return new (md_alloc()) Unlabeled_session_component(*file);
		}

		void _destroy_session(Session_component *session)
		{
			Log_file *file = session->file();
			destroy(md_alloc(), session);
			if (file->client_count() < 1) {
				_log_files.remove(file);
				destroy(env()->heap(), file);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Server::Entrypoint &ep, Allocator &alloc)
		:
			Genode::Root_component<Session_component>(&ep.rpc_ep(), &alloc),
			_write_alloc(env()->heap()),
			_fs(_write_alloc, TX_BUF_SIZE)
		{ }

};

struct Fs_log::Main
{
	Server::Entrypoint &ep;

	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root_component root { ep, sliced_heap };

	Main(Server::Entrypoint &ep)
	: ep(ep)
	{ Genode::env()->parent()->announce(ep.manage(root)); }
};


/************
 ** Server **
 ************/

namespace Server {

	char const* name() { return "fs_log_ep"; }

	size_t stack_size() { return 3*512*sizeof(long); }

	void construct(Entrypoint &ep) { static Fs_log::Main inst(ep); }

}
