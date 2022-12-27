/*
 * \brief  Definitions for FS front-end
 * \author Sebastian Sumpf
 * \date   2014-01-22
 */

/*
 * Copyright (C) 2014-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
 * Device where to mount the block device
 */
#define GENODE_MOUNT_DIR     "/mnt"


void rump_io_backend_init();

/**
 * Sync I/O back-end with underlying Genode subsystems
 */
extern "C" void rump_io_backend_sync();

/**
 * Return true if an I/O backend-request is pending
 *
 * While waiting for the completion of an I/O request, Genode I/O signals
 * may occur, in particular the periodic sync signal of the vfs_rump plugin.
 * Under this condition, however, it is not safe to call into the rump
 * kernel ('rump_sys_sync'). The return value allows the caller to skip
 * the periodic sync in this case.
 */
bool rump_io_backend_blocked_for_io();

#endif /* _INCLUDE__RUMP_FS__FS_H_ */
