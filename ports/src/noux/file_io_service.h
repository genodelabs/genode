/*
 * \brief  Interface for operations provided by file I/O service
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__FILE_IO_SERVICE_H_
#define _NOUX__FILE_IO_SERVICE_H_

namespace Noux {

	class Vfs_handle;
	class Sysio;

	/**
	 * Abstract file-system interface
	 */
	struct File_io_service
	{
		virtual bool     write(Sysio *sysio, Vfs_handle *vfs_handle) = 0;
		virtual bool      read(Sysio *sysio, Vfs_handle *vfs_handle) = 0;
		virtual bool ftruncate(Sysio *sysio, Vfs_handle *vfs_handle) = 0;

		/**
		 * This method is only needed in file-systems which actually implement
		 * a device and is therefore false by default.
		 */
		virtual bool     ioctl(Sysio *sysio, Vfs_handle *vfs_handle) { return false; }

		/**
		 * Return true if an unblocking condition of the file is satisfied
		 *
		 * \param rd  if true, check for data available for reading
		 * \param wr  if true, check for readiness for writing
		 * \param ex  if true, check for exceptions
		 */
		virtual bool check_unblock(Vfs_handle *vfs_handle,
		                           bool rd, bool wr, bool ex)
		{ return true; }

		virtual void register_read_ready_sigh(Vfs_handle *vfs_handle,
		                                      Signal_context_capability sigh)
		{ }
	};
}

#endif /* _NOUX__FILE_IO_SERVICE_H_ */
