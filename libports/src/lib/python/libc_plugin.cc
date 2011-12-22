/**
 * \brief  Libc back-end for Python
 * \author Sebastian Sumpf
 * \date   2010-02-17
 */

/*
 * Copyright (C) 2010-2011 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <rom_session/connection.h>
#include <dataspace/client.h>
#include <libc-plugin/fd_alloc.h>

namespace {
	
	class Plugin_context : public Libc::Plugin_context
	{
		private:

			Genode::Dataspace_capability _ds_cap;
			::size_t                     _ds_size;
			::off_t                      _offs;
			Genode::addr_t               _base;

		public:

			Plugin_context(Genode::Dataspace_capability ds_cap)
			: _ds_cap(ds_cap), _offs(0)
			{
				_ds_size = Genode::Dataspace_client(ds_cap).size();
				_base    = Genode::env()->rm_session()->attach(ds_cap);
			}

			ssize_t read(void *buf, ::size_t count)
			{
				if (_offs >= (::off_t)_ds_size)
					return -1;

				if (_offs + count > _ds_size)
					count = _ds_size - _offs;

				Genode::memcpy(buf, (void *)(_base + _offs), count);
				_offs += count;
				return count;
			}
	};


	static inline Plugin_context *context(Libc::File_descriptor *fd)
	{
		return static_cast<Plugin_context *>(fd->context);
	}


	struct Plugin : Libc::Plugin
	{
		Plugin()
		{
			PDBG("Python libc plugin");
		}

		bool supports_open(const char*, int) { return true; }

		Libc::File_descriptor *open(const char *pathname, int flags)
		{
			using namespace Genode;

			Dataspace_capability ds_cap;
			const char *_pathname = (pathname[0] == '/') ? pathname + 1 : pathname;

			try {
				/* Open the file dataspace */
				Rom_connection rom(_pathname);
				rom.on_destruction(Rom_connection::KEEP_OPEN);
				ds_cap = rom.dataspace();
			}
			catch (...) {
				PERR("could not open file: %s", pathname);
				return 0;
			}

			Plugin_context *context = new (env()->heap()) Plugin_context(ds_cap);
			return Libc::file_descriptor_allocator()->alloc(this, context);
		}

		ssize_t read(Libc::File_descriptor *fdo, void *buf, ::size_t count)
		{
			return context(fdo)->read(buf, count);
		}

		ssize_t _read(Libc::File_descriptor *fdo, void *buf, ::size_t count)
		{
			return read(fdo, buf, count);
		}
	};
}


void create_libc_plugin()
{
	static Plugin plugin;
}

