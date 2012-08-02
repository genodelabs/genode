/*
 * \brief  Interface for operations provided by file I/O service
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
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
	};
}

#endif /* _NOUX__FILE_IO_SERVICE_H_ */
