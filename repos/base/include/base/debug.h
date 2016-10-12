/*
 * \brief  Debugging output function
 * \author Emery Hemingway
 * \date   2016-10-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__DEBUG_H_
#define _INCLUDE__BASE__DEBUG_H_
#ifndef GENODE_RELEASE

#include <base/log.h>

#define PDBG(...) \
    Genode::log("\033[33m", __PRETTY_FUNCTION__, "\033[0m ", ##__VA_ARGS__)

#endif /* GENODE_RELEASE */
#endif /* _INCLUDE__BASE__DEBUG_H_ */
