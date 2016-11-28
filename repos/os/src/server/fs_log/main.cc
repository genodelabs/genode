/*
 * \brief  Server that writes log messages to a file system.
 * \author Emery Hemingway
 * \date   2015-05-13
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/heap.h>
#include <file_system_session/connection.h>
#include <file_system/util.h>
#include <os/path.h>
#include <os/session_policy.h>
#include <base/heap.h>
#include <base/attached_rom_dataspace.h>
#include <root/component.h>
#include <base/component.h>
#include <base/log.h>

/* Local includes */
#include "session.h"

namespace Fs_log {

	using namespace Genode;
	using namespace File_system;

	class  Root_component;

	enum {
		PACKET_SIZE = Log_session::String::MAX_SIZE,
		 QUEUE_SIZE = File_system::Session::TX_QUEUE_SIZE,
		TX_BUF_SIZE = PACKET_SIZE * (QUEUE_SIZE+2)
	};

	typedef Genode::Path<File_system::MAX_PATH_LEN> Path;

}


class Fs_log::Root_component :
	public Genode::Root_component<Fs_log::Session_component>
{
	private:

		Genode::Env                    &_env;
		Genode::Attached_rom_dataspace  _config_rom { _env, "config" };
		Genode::Heap                    _heap { _env.ram(), _env.rm() };
		Allocator_avl                   _tx_alloc { &_heap };
		File_system::Connection         _fs
			{ _env, _tx_alloc, "", "/", true, TX_BUF_SIZE };

		void _update_config() { _config_rom.update(); }

		Genode::Signal_handler<Root_component> _config_handler
			{ _env.ep(), *this, &Root_component::_update_config };

	protected:

		Session_component *_create_session(const char *args)
		{
			using namespace File_system;

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota").aligned_size();
			if (ram_quota < sizeof(Session_component))
				throw Root::Quota_exceeded();

			Path dir_path;
			char file_name[MAX_NAME_LEN];

			Session_label const session_label = label_from_args(args);
			char const *label_str = session_label.string();
			char const *label_prefix = "";
			bool truncate = false;

			try {
				Session_policy policy(session_label, _config_rom.xml());
				truncate = policy.attribute_value("truncate", truncate);
				bool merge = policy.attribute_value("merge", false);

				/* only a match on 'label_prefix' can be merged */
				if (merge && policy.has_type("policy")
				 && (!(policy.has_attribute("label")
				    || policy.has_attribute("label_suffix"))))
				{
					/*
					 * split the label between what will be the log file
					 * and what will be prepended to messages in the file
					 */
					size_t offset = policy.attribute("label_prefix").value_size();
					for (size_t i = offset; i < session_label.length()-4; ++i) {
						if (strcmp(label_str+i, " -> ", 4))
							continue;

						label_prefix = label_str+i+4;
						{
							char tmp[128];
							strncpy(tmp, label_str, min(sizeof(tmp), i+1));
							dir_path = path_from_label<Path>(tmp);
						}
						break;
					}
					if (dir_path == "/")
						dir_path = path_from_label<Path>(label_str);

				} else if (!policy.has_type("default-policy")) {
					dir_path = path_from_label<Path>(label_str);
				}

			} catch (Session_policy::No_policy_defined) {
				dir_path = path_from_label<Path>(label_str);
			}

			if (dir_path == "/") {
				strncpy(file_name, "log", sizeof(file_name));
				label_prefix = label_str;
			} else {
				dir_path.append(".log");
				strncpy(file_name, dir_path.last_element(), sizeof(file_name));
				dir_path.strip_last_element();
				dir_path.remove_trailing('/');
			}

			char const *errstr;
			try {

				Dir_handle   dir_handle = ensure_dir(_fs, dir_path.base());
				Handle_guard dir_guard(_fs, dir_handle);
				File_handle  handle;

				try {
					handle = _fs.file(dir_handle, file_name,
					                  File_system::WRITE_ONLY, false);

					/* don't truncate at every new child session */
					if (truncate && (strcmp(label_prefix, "") == 0))
						_fs.truncate(handle, 0);

				} catch (File_system::Lookup_failed) {
					handle = _fs.file(dir_handle, file_name,
					                  File_system::WRITE_ONLY, true);
				}

				return new (md_alloc()) Session_component(_fs, handle, label_prefix);
			}

			catch (Permission_denied) {
				errstr = "permission denied"; }
			catch (No_space) {
				errstr = "file system out of space"; }
			catch (Out_of_metadata) {
				errstr = "file system server out of metadata"; }
			catch (Invalid_name) {
				errstr = "invalid path"; }
			catch (Name_too_long) {
				errstr = "name too long"; }
			catch (...) {
				errstr = "unhandled error"; }

			Genode::error("cannot open log file ",
			              (char const *)dir_path.base(),
			              ", ", errstr);
			throw Root::Unavailable();
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Genode::Env &env, Genode::Allocator &md_alloc)
		:
			Genode::Root_component<Session_component>(&env.ep().rpc_ep(), &md_alloc),
			_env(env)
		{
			_config_rom.sigh(_config_handler);

			/* fill the ack queue with packets so sessions never need to alloc */
			File_system::Session::Tx::Source &source = *_fs.tx();
			for (int i = 0; i < QUEUE_SIZE-1; ++i)
				source.submit_packet(source.alloc_packet(PACKET_SIZE));

			env.parent().announce(env.ep().manage(*this));
		}

};


void Component::construct(Genode::Env &env)
{
	static Genode::Sliced_heap sliced_heap { env.ram(), env.rm() };
	static Fs_log::Root_component root { env, sliced_heap };
}
