/*
 * \brief  Definitions for FS front-end
 * \author Sebastian Sumpf
 * \date   2014-01-22
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#ifndef _INCLUDE__RUMP_FS__FS_H_
#define _INCLUDE__RUMP_FS__FS_H_

/**
 * File to upon the back-end will open a block session
 */
#define GENODE_BLOCK_SESSION "block_session"

/**
 * Device to create within rump
 */
#define GENODE_DEVICE        "/genode"


/**
 * Sync I/O back-end with underlying Genode subsystems
 */
void rump_io_backend_sync();

#endif /* _INCLUDE__RUMP_FS__FS_H_ */
