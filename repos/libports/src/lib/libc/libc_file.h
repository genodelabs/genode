/*
 * \brief  File-operation utilities
 * \author Christian Helmuth
 * \author Emery Hemingway
 * \date   2015-06-30
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_FILE_H_
#define _LIBC_FILE_H_

/* Genode includes */
#include <base/log.h>

/* Genode-specific libc interfaces */
#include <libc-plugin/fd_alloc.h>
#include <libc-plugin/plugin_registry.h>


enum { INVALID_FD = -1 };

/**
 * Find plugin responsible for the specified libc file descriptor
 *
 * \param func_name  function name of the caller for printing an error message
 */
static inline Libc::File_descriptor *libc_fd_to_fd(int libc_fd, const char *func_name)
{
	Libc::File_descriptor *fd =
		Libc::file_descriptor_allocator()->find_by_libc_fd(libc_fd);
	if (!fd)
		Genode::error("no plugin found for ", func_name, "(", libc_fd, ")");
	return fd;
}


/**
 * Generate body of wrapper function taking a file descriptor as first argument
 */
#define FD_FUNC_WRAPPER_GENERIC(result_stm, result_err_val, func_name, libc_fd, ...)	\
{																		\
	File_descriptor *fd = libc_fd_to_fd(libc_fd, #func_name);			\
	if (!fd || !fd->plugin) {											\
		errno = EBADF;													\
		result_stm result_err_val;											\
	} else																\
		result_stm fd->plugin->func_name(fd, ##__VA_ARGS__ ); 			\
}

#define FD_FUNC_WRAPPER(func_name, libc_fd, ...) \
	FD_FUNC_WRAPPER_GENERIC(return, INVALID_FD, func_name, libc_fd, ##__VA_ARGS__ )

/**
 * Generate body of wrapper function taking a path name as first argument
 */
#define FNAME_FUNC_WRAPPER_GENERIC(result_stm, func_name, path, ...)						\
{																							\
	Plugin *plugin  = plugin_registry()->get_plugin_for_##func_name(path, ##__VA_ARGS__);	\
	if (!plugin) {																			\
		Genode::error("no plugin found for ", #func_name, "(\"", Genode::Cstring(path), "\")");\
		errno = ENOSYS;																		\
		result_stm -1;																		\
	} else																					\
		result_stm plugin->func_name(path, ##__VA_ARGS__);									\
}

#define FNAME_FUNC_WRAPPER(func_name, path, ...) \
	FNAME_FUNC_WRAPPER_GENERIC(return, func_name, path, ##__VA_ARGS__ )

#endif /* _LIBC_FILE_H_ */
