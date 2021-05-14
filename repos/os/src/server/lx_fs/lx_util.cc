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

/* local includes */
#include "lx_util.h"


int File_system::access_mode(File_system::Mode const &mode)
{
	switch (mode) {
	case STAT_ONLY:
	case READ_ONLY:  return O_RDONLY;
	case WRITE_ONLY: return O_WRONLY;
	case READ_WRITE: return O_RDWR;
	}

	return O_RDONLY;
}


Lx_fs::Path_string Lx_fs::absolute_root_dir(char const *root_path)
{
	char cwd[PATH_MAX];
	char real_path[PATH_MAX];

	getcwd(cwd, PATH_MAX);

	realpath(Path_string { cwd, "/", root_path }.string(), real_path);

	return Path_string { real_path };
}
