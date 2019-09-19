/*
 * \brief  Globally available accessors to Libc kernel functionality
 * \author Norman Feske
 * \date   2019-09-18
 */

/*
 * Copyright (C) 2016-2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__LEGACY_H_
#define _LIBC__INTERNAL__LEGACY_H_

/* Genode includes */
#include <vfs/vfs_handle.h> /* needed for 'watch' */

/* libc-internal includes */
#include <internal/types.h>

namespace Libc {

	/*
	 * XXX  called only by 'Libc::Vfs_plugin::read'
	 */
	void dispatch_pending_io_signals();

	/**
	 * Get watch handle for given path
	 *
	 * \param path  path that should be be watched
	 *
	 * \return      point to the watch handle object or a nullptr
	 *              when the watch operation failed
	 *
	 * XXX  only needed by time.cc
	 */
	Vfs::Vfs_watch_handle *watch(char const *path);

	/*
	 * XXX  this function is solely needed to support noux fork mechanism
	 */
	void schedule_suspend(void (*) ());

	/**
	 * Access libc configuration Xml_node.
	 */
	Xml_node libc_config();
}

#endif /* _LIBC__INTERNAL__LEGACY_H_ */
