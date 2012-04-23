/*
 * \brief  Directory-service interface
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__DIRECTORY_SERVICE_H_
#define _NOUX__DIRECTORY_SERVICE_H_

/* Genode includes */
#include <dataspace/capability.h>

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux {

	class Vfs_handle;

	/**
	 * Abstract interface to stateless directory service
	 */
	struct Directory_service
	{
		virtual Dataspace_capability dataspace(char const *path) = 0;
		virtual void release(Dataspace_capability) = 0;

		virtual Vfs_handle *open(Sysio *sysio, char const *path) = 0;

		virtual bool   stat(Sysio *sysio, char const *path) = 0;
		virtual bool dirent(Sysio *sysio, char const *path) = 0;

		virtual void close(Vfs_handle *handle) = 0;
	};
}

#endif /* _NOUX__DIRECTORY_SERVICE_H_ */
