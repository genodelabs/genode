/*
 * \brief  Mode utilities
 * \author Josef Soentgen
 * \date   2013-11-26
 */

#ifndef _MODE_UTIL_H_
#define _MODE_UTIL_H_

/* Genode includes */
#include <file_system_session/file_system_session.h>

/* libc includes */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

#endif /* _MODE_UTIL_H_ */
