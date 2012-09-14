/*
 * \brief  Manager for file descriptors of one child
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__FILE_DESCRIPTOR_REGISTRY_H_
#define _NOUX__FILE_DESCRIPTOR_REGISTRY_H_

/* Noux includes */
#include <io_channel.h>

namespace Noux {

	class File_descriptor_registry
	{
		public:

			enum { MAX_FILE_DESCRIPTORS = 64 };

		private:

			struct {
				bool                       allocated;
				Shared_pointer<Io_channel> io_channel;
			} _fds[MAX_FILE_DESCRIPTORS];

			bool _is_valid_fd(int fd) const
			{
				return (fd >= 0) && (fd < MAX_FILE_DESCRIPTORS);
			}

			bool _find_available_fd(int *fd) const
			{
				/* allocate the first free file descriptor */
				for (unsigned i = 0; i < MAX_FILE_DESCRIPTORS; i++)
					if (_fds[i].allocated == false) {
						*fd = i;
						return true;
					}
				return false;
			}

			void _assign_fd(int fd, Shared_pointer<Io_channel> &io_channel)
			{
				_fds[fd].io_channel = io_channel;
				_fds[fd].allocated  = true;
			}

			void _reset_fd(int fd)
			{
				_fds[fd].io_channel = Shared_pointer<Io_channel>();
				_fds[fd].allocated  = false;
			}

		public:

			File_descriptor_registry()
			{
				flush();
			}

			/**
			 * Associate I/O channel with file descriptor
			 *
			 * \return noux file descriptor used for the I/O channel
			 */
			int add_io_channel(Shared_pointer<Io_channel> io_channel, int fd = -1)
			{
				if ((fd == -1) && !_find_available_fd(&fd)) {
					PERR("Could not allocate file descriptor");
					return -1;
				}

				if (!_is_valid_fd(fd)) {
					PERR("File descriptor %d is out of range", fd);
					return -2;
				}

				_assign_fd(fd, io_channel);
				return fd;
			}

			void remove_io_channel(int fd)
			{
				if (!_is_valid_fd(fd))
					PERR("File descriptor %d is out of range", fd);
				else
					_reset_fd(fd);
			}

			bool fd_in_use(int fd) const
			{
				return (_is_valid_fd(fd) && _fds[fd].io_channel);
			}

			Shared_pointer<Io_channel> io_channel_by_fd(int fd) const
			{
				if (!fd_in_use(fd)) {
					PWRN("File descriptor %d is not open", fd);
					return Shared_pointer<Io_channel>();
				}
				return _fds[fd].io_channel;
			}

			void flush()
			{
				/* close all file descriptors */
				for (unsigned i = 0; i < MAX_FILE_DESCRIPTORS; i++)
					_reset_fd(i);
			}
	};
}

#endif /* _NOUX__FILE_DESCRIPTOR_REGISTRY_H_ */
