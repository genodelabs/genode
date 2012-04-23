/*
 * \brief  File-system related interfaces for Noux
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 gENODe Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__FILE_SYSTEM_H_
#define _NOUX__FILE_SYSTEM_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>
#include <util/xml_node.h>

/* Noux includes */
#include <directory_service.h>
#include <file_io_service.h>
#include <noux_session/sysio.h>

namespace Noux {

	class File_system : public Directory_service, public File_io_service,
	                    public List<File_system>::Element
	{
		private:

			char _mount_point[Sysio::MAX_PATH_LEN];

			/**
			 * Strip any number of trailing slashes
			 */
			void _strip_trailing_slashes_from_mount_point()
			{
				size_t len;
				while ((len = strlen(_mount_point)) && (_mount_point[len - 1] == '/'))
					_mount_point[len - 1] = 0;
			}

		public:

			File_system(Xml_node config)
			{
				enum { TYPE_MAX_LEN = 64 };
				char type[TYPE_MAX_LEN];
				config.type_name(type, sizeof(type));

				config.attribute("at").value(_mount_point, sizeof(_mount_point));
				_strip_trailing_slashes_from_mount_point();

				PINF("created %s file system at \"%s\"", type, _mount_point);
			}

			File_system(char const *mount_point)
			{
				strncpy(_mount_point, mount_point, sizeof(_mount_point));
				_strip_trailing_slashes_from_mount_point();
			}

			/**
			 * Return file-system-local path for the given global path
			 *
			 * This function checks if the global path lies within the mount
			 * point of the file system. If yes, it returns the sub string of
			 * the global path that corresponds to the path relative to the
			 * file system.
			 *
			 * \return pointer to file-system relative path name within
			 *         the 'global_path' string, or
			 *         0 if global path lies outside the file system
			 */
			char const *local_path(char const *global_path)
			{
				size_t const mount_point_len = strlen(_mount_point);

				if (strlen(global_path) < mount_point_len)
					return 0;

				if (strcmp(global_path, _mount_point, mount_point_len))
					return 0;

				return global_path + mount_point_len;
			}
	};
}

#endif /* _NOUX__FILE_SYSTEM_H_ */
