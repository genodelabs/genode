/*
 * \brief  Stdio filesystem
 * \author Josef Soentgen
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__STDIO_FILE_SYSTEM_H_
#define _NOUX__STDIO_FILE_SYSTEM_H_

/* Genode includes */
#include <base/printf.h>
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include "file_system.h"
#include "terminal_connection.h"


namespace Noux {

	class Stdio_file_system : public File_system
	{
		private:

			enum { FILENAME_MAX_LEN = 64 };
			char _filename[FILENAME_MAX_LEN];

			Terminal::Session_client *_terminal;
			bool _echo;


			bool _is_root(const char *path)
			{
				return (strcmp(path, "") == 0) || (strcmp(path, "/") == 0);
			}

			bool _is_stdio_file(const char *path)
			{
				return (strlen(path) == (strlen(_filename) + 1)) &&
					   (strcmp(&path[1], _filename) == 0);
			}

		public:

			Stdio_file_system(Xml_node config,
			                  Terminal::Session_client *terminal = terminal())
			:
				_terminal(terminal),
				_echo(true)
			{
				_filename[0] = '\0';

				try { config.attribute("name").value(_filename, sizeof(_filename)); }
				catch (...) { }

			}


			/*********************************
			 ** Directory-service interface **
			 *********************************/

			Dataspace_capability dataspace(char const *path)
			{
				/* not supported */
				return Dataspace_capability();
			}

			void release(char const *path, Dataspace_capability ds_cap)
			{
				/* not supported */
			}

			bool stat(Sysio *sysio, char const *path)
			{
				Sysio::Stat st = { 0, 0, 0, 0, 0, 0 };

				if (_is_root(path))
					st.mode = Sysio::STAT_MODE_DIRECTORY;
				else if (_is_stdio_file(path)) {
					st.mode = Sysio::STAT_MODE_CHARDEV;
				} else {
					sysio->error.stat = Sysio::STAT_ERR_NO_ENTRY;
					return false;
				}

				sysio->stat_out.st = st;
				return true;
			}

			bool dirent(Sysio *sysio, char const *path, off_t index)
			{
				if (_is_root(path)) {
					if (index == 0) {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_CHARDEV;
						strncpy(sysio->dirent_out.entry.name,
						        _filename,
						        sizeof(sysio->dirent_out.entry.name));
					} else {
						sysio->dirent_out.entry.type = Sysio::DIRENT_TYPE_END;
					}

					return true;
				}

				return false;
			}

			size_t num_dirent(char const *path)
			{
				if (_is_root(path))
					return 1;
				else
					return 0;
			}

			bool is_directory(char const *path)
			{
				if (_is_root(path))
					return true;

				return false;
			}

			char const *leaf_path(char const *path)
			{
				return path;
			}

			Vfs_handle *open(Sysio *sysio, char const *path)
			{
				if (!_is_stdio_file(path)) {
					sysio->error.open = Sysio::OPEN_ERR_UNACCESSIBLE;
					return 0;
				}

				return new (env()->heap()) Vfs_handle(this, this, 0);
			}

			bool unlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool readlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool rename(Sysio *sysio, char const *from_path, char const *to_path)
			{
				/* not supported */
				return false;
			}

			bool mkdir(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			bool symlink(Sysio *sysio, char const *path)
			{
				/* not supported */
				return false;
			}

			/***************************
			 ** File_system interface **
			 ***************************/

			static char const *name() { return "stdio"; }


			/********************************
			 ** File I/O service interface **
			 ********************************/

			bool write(Sysio *sysio, Vfs_handle *handle)
			{
				sysio->write_out.count = _terminal->write(sysio->write_in.chunk, sysio->write_in.count);

				return true;
			}

			bool read(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				sysio->read_out.count = _terminal->read(sysio->read_out.chunk, sysio->read_in.count);

				if (_echo)
					_terminal->write(sysio->read_out.chunk, sysio->read_in.count);

				return true;
			}

			bool ftruncate(Sysio *sysio, Vfs_handle *vfs_handle) { return true; }

			bool ioctl(Sysio *sysio, Vfs_handle *vfs_handle)
			{
				switch (sysio->ioctl_in.request) {

				case Sysio::Ioctl_in::OP_TIOCSETAF:
					{
						if (sysio->ioctl_in.argp & (Sysio::Ioctl_in::VAL_ECHO)) {
							_echo = true;
						}
						else {
							_echo = false;
						}

						return true;
					}

				case Sysio::Ioctl_in::OP_TIOCSETAW:
					{
						PDBG("OP_TIOCSETAW not implemented");
						return false;
					}

				default:

					PDBG("invalid ioctl(request=0x%x), %d", sysio->ioctl_in.request,
					     Sysio::Ioctl_in::OP_TIOCSETAW);
					break;
				}

				return false;
			}
	};
}

#endif /* _NOUX__STDIO_FILE_SYSTEM_H_ */
