/*
 * \brief  Linux utilities
 * \author Christian Helmuth
 * \author Pirmin Duss
 * \date   2013-11-11
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LX_UTIL_H_
#define _LX_UTIL_H_

/* Genode includes */
#include <file_system_session/file_system_session.h>
#include <util/string.h>

/* Linux includes */
#define _FILE_OFFSET_BITS 64
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>


namespace File_system {
	int access_mode(File_system::Mode const &mode);
}


namespace Lx_fs {

	using namespace Genode;

	using Path_string = Genode::String<File_system::MAX_PATH_LEN>;

	/*
	 * Calculate the absolute path that the root of a File_system session is
	 * located at.
	 *
	 * @param  root_path  root path specified in the policy of a File_system
	 *                    session.
	 *
	 */
	Path_string absolute_root_dir(char const *root_path);
}

#endif  /* _LX_UTIL_H_ */
