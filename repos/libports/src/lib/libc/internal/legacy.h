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

	/*
	 * XXX  this function is solely needed to support noux fork mechanism
	 */
	void schedule_suspend(void (*) ());
}

#endif /* _LIBC__INTERNAL__LEGACY_H_ */
