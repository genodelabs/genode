/*
 * \brief  Rom-session server for ISO file systems
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <rom_session/connection.h>
#include <root/component.h>
#include <rm_session/connection.h>
#include <util/avl_string.h>
#include <util/misc_math.h>
#include <os/attached_ram_dataspace.h>
#include <base/session_label.h>

/* local includes */
#include "iso9660.h"
#include "backing_store.h"

using namespace Genode;


/*****************
 ** ROM service **
 *****************/

namespace Iso {

	typedef Backing_store<Genode::off_t> Backing_store;
	typedef Avl_string<PATH_LENGTH>      File_base;

	/**
	 * File abstraction
	 */
	class File : public File_base
	{
		private:

			File_info                *_info;
			Attached_ram_dataspace    _ds;

		public:

			File(char *path) : File_base(path),
				_info(Iso::file_info(path)),
				_ds(env()->ram_session(), align_addr(_info->page_sized(), 12))
			{
				Iso::read_file(_info, 0, _ds.size(), _ds.local_addr<void>());
			}
			
			~File()
			{
				destroy(env()->heap(), _info);
			}

			Dataspace_capability dataspace() { return _ds.cap(); }


			/**************************
			 ** File cache interface **
			 **************************/

			/** 
			 * File cache that holds files in order to re-use
			 * them in different sessions that request already cached files
			 */
			static Avl_tree<Avl_string_base> *cache()
			{
				static Avl_tree<Avl_string_base> _avl;
				return &_avl;
			}

			static File *scan_cache(const char *path) {
				return static_cast<File *>(cache()->first() ?
				                           cache()->first()->find_by_name(path) :
				                           0);
			}
	};


	class Rom_component : public Genode::Rpc_object<Rom_session>
	{
		private:

			File *_file;

		public:

			Rom_dataspace_capability dataspace() {
				return static_cap_cast<Rom_dataspace>(_file->dataspace()); }

			void sigh(Signal_context_capability) { }

			Rom_component(char *path)
			{
				if ((_file = File::scan_cache(path))) {
					Genode::log("cache hit for file ", Genode::Cstring(path));
					return;
				}

				_file = new(env()->heap()) File(path);
				Genode::log("request for file ", Genode::Cstring(path));

				File::cache()->insert(_file);
			}
	};


	typedef Root_component<Rom_component> Root_component;

	class Root : public Root_component
	{
		private:

			char _path[PATH_LENGTH];

		protected:

			Rom_component *_create_session(const char *args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").ulong_value(0);
				size_t session_size =  sizeof(Rom_component) + sizeof(File_info);
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				Session_label const label = label_from_args(args);
				strncpy(_path, label.last_element().string(), sizeof(_path));

				if (verbose)
					Genode::log("Request for file ", Cstring(_path), " len ", strlen(_path));

				try {
					return new (md_alloc()) Rom_component(_path);
				}
				catch (Io_error)       { throw Root::Unavailable(); }
				catch (Non_data_disc)  { throw Root::Unavailable(); }
				catch (File_not_found) { throw Root::Invalid_args(); }
			}

		public:

			Root(Rpc_entrypoint *ep, Allocator *md_alloc)
			:
				Root_component(ep, md_alloc)
			{ }
	};
}


int main()
{
	/* initialize ROM service */
	enum { STACK_SIZE = sizeof(long)*4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "iso9660_ep");

	static Iso::Root root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&root));

	sleep_forever();
	return 0;
}


