/*
 * \brief  I/O-port session capability type
 * \author Norman Feske
 * \date   2008-08-16
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__IO_PORT_SESSION__CAPABILITY_H_
#define _INCLUDE__IO_PORT_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <io_port_session/io_port_session.h>

namespace Genode { using Io_port_session_capability = Capability<Io_port_session>; }

#endif /* _INCLUDE__IO_PORT_SESSION__CAPABILITY_H_ */
