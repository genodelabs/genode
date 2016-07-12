/*
 * \brief  Server-side signal-source interface
 * \author Norman Feske
 * \date   2011-04-12
 *
 * This class solely exists as a hook to insert platform-specific
 * implementation bits (i.e., for the NOVA base platform, there exists
 * an enriched version of this class).
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__RPC_OBJECT_H_
#define _INCLUDE__SIGNAL_SOURCE__RPC_OBJECT_H_

#include <base/rpc_server.h>
#include <signal_source/signal_source.h>

namespace Genode { struct Signal_source_rpc_object; }

struct Genode::Signal_source_rpc_object : Rpc_object<Signal_source> { };

#endif /* _INCLUDE__SIGNAL_SOURCE__RPC_OBJECT_H_ */
