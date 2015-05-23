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
#include "session.h"

namespace Fs_log {

	using namespace Genode;

	class  Root_component;
	struct Main;

	enum {
		BLOCK_SIZE  = Log_session::String::MAX_SIZE,
		QUEUE_SIZE  = File_system::Session::TX_QUEUE_SIZE,
		TX_BUF_SIZE = BLOCK_SIZE * QUEUE_SIZE
	};

}

class Fs_log::Root_component : public Genode::Root_component<Fs_log::Session_component>
{
	private:

		Allocator_avl                             _write_alloc;
		File_system::Connection                   _fs;

		File_system::File_handle open_file(File_system::Dir_handle &dir_handle,
		                                   char const *name)
		{
			try {
				return _fs.file(dir_handle, name, File_system::WRITE_ONLY, false);
			} catch (File_system::Lookup_failed) {
				return _fs.file(dir_handle, name, File_system::WRITE_ONLY, true);
			}
		}

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace File_system;

			char path[MAX_PATH_LEN];
			path[0] = '/';
			char name[MAX_NAME_LEN];

			Session_label session_label(args);
			strncpy(path+1, session_label.string(), sizeof(path)-1);

			bool truncate = false;
			try {
				Session_policy policy(session_label);
				try {
					truncate = policy.attribute("truncate").has_value("yes");
				} catch (Xml_node::Nonexistent_attribute) { }

			} catch (Session_policy::No_policy_defined) { }

			size_t len = strlen(path);
			size_t start = 1;
			for (size_t i = 1; i < len;) {
				/* Replace any slashes in label elements. */
				if (path[i] == '/') path[i] = '_';
				if (strcmp(" -> ", path+i, 4) == 0) {
					path[i++] = '/';
					strncpy(path+i, path+i+3, sizeof(path)-i);
					start = i;
					i += 3;
				} else ++i;
			}

			snprintf(name, sizeof(name), "%s.log", path+start);
			path[(start == 1) ? start : start-1] = '\0';

			/* Rewrite any slashes in the name. */
			for (char *p = name; *p; ++p)
				if (*p == '/') *p = '_';

			File_handle file_handle;
			seek_off_t offset = 0;
			try {
				Dir_handle dir_handle = ensure_dir(_fs, path);
				Handle_guard dir_guard(_fs, dir_handle);

				file_handle = open_file(dir_handle, name);

				if (truncate)
					_fs.truncate(file_handle, 0);
				else
					offset = _fs.status(file_handle).size;

			} catch (Permission_denied) {
				PERR("%s/%s: permission denied", path, name);
				throw Root::Unavailable();

			} catch (Name_too_long) {
				PERR("%s/%s: name too long", path, name);
				throw Root::Unavailable();

			} catch (No_space) {
				PERR("%s/%s: no space", path, name);
				throw Root::Unavailable();

			} catch (Out_of_node_handles) {
				PERR("%s/%s: out of node handles", path, name);
				throw Root::Unavailable();

			} catch (Invalid_name) {
				PERR("%s/%s: invalid_name", path, name);
				throw Root::Unavailable();

			} catch (Size_limit_reached) {
				PERR("%s/%s: size limit reached", path, name);
				throw Root::Unavailable();

			} catch (...) {
				PERR("%s/%s: unknown error", path, name);
				throw Root::Unavailable();
			}

			return new (md_alloc()) Session_component(_fs, file_handle, offset);
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
