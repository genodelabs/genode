/**
 * \brief  Definitions for RUMP cgd
 * \author Josef Soentgen
 * \date   2014-04-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__RUMP_CGD__CGD_H_
#define _INCLUDE__RUMP_CGD__CGD_H_

#include <rump_cgd/device.h>

/**
 * File to upon the back-end will open a block session
 */
#define GENODE_BLOCK_SESSION "block_session"

/**
 * Device to create within rump
 */
#define GENODE_DEVICE        "/genode"

/**
 * Encryption algorithm used by cgd
 */
#define CGD_ALGORITHM        "aes-cbc"

/**
 * Initialization vector method used by cgd
 */
#define CGD_IVMETHOD         "encblkno1"

/**
 * Key length in bits
 */
#define CGD_KEYLEN           256

/**
 * Sync I/O back-end with underlying Genode subsystems
 */
void rump_io_backend_sync();

#endif /* _INCLUDE__RUMP_CGD__CGD_H_ */
