/*
 * \brief  I/O-memory session capability type
 * \author Norman Feske
 * \date   2008-08-16
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IO_MEM_SESSION__CAPABILITY_H_
#define _INCLUDE__IO_MEM_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <io_mem_session/io_mem_session.h>

namespace Genode { typedef Capability<Io_mem_session> Io_mem_session_capability; }

#endif /* _INCLUDE__IO_MEM_SESSION__CAPABILITY_H_ */
