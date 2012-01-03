/*
 * \brief  Root file system
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__ROOT_FILE_SYSTEM_H_
#define _NOUX__ROOT_FILE_SYSTEM_H_

/* Genode includes */
#include <base/printf.h>
#include <base/lock.h>

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux {

	class Root_file_system : public File_system
	{
		private:

			Genode::Lock _lock;

		public:

			Root_file_system() : File_system("/") { }


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			bool stat(Sysio *sysio, char const *path)
			{
				sysio->stat_out.st.size = 1234;
				sysio->stat_out.st.mode = Sysio::STAT_MODE_DIRECTORY | 0755;
				sysio->stat_out.st.uid  = 13;
				sysio->stat_out.st.gid  = 14;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path)
			{
				Genode::Lock::Guard guard(_lock);

				int const index = sysio->dirent_in.index;
				if (index) {
					sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					return true;
				}
				sysio->dirent_out.entry.fileno = 13;

				Genode::strncpy(sysio->dirent_out.entry.name, "test",
				                sizeof(sysio->dirent_out.entry.name));

				sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_DIRECTORY;
				return true;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				Genode::Lock::Guard guard(_lock);

				if (Genode::strcmp(path, "/") == 0)
					return new (Genode::env()->heap()) Vfs_handle(this, this, 0);

				return 0;
			}

			void close(Vfs_handle *handle)
			{
				Genode::Lock::Guard guard(_lock);
				Genode::destroy(Genode::env()->heap(), handle);
			}


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *handle) { return false; }
	};
}

#endif /* _NOUX__ROOT_FILE_SYSTEM_H_ */
