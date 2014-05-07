/*
 * \brief  Linux utilities
 * \author Christian Helmuth
 * \date   2013-11-11
 */

#ifndef _LX_UTIL_H_
#define _LX_UTIL_H_

/* Genode includes */
#include <file_system_session/file_system_session.h>

/* Linux includes */
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>


namespace File_system {
	int access_mode(File_system::Mode const &mode);
}


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

#endif
