/*
 * \brief  Generic implementation parts of the signaling framework
 * \author Norman Feske
 * \date   2017-05-10
 *
 * This dummy is used on base platforms that transmit signals directly via
 * the kernel instead of using core's signal-source entrypoint as a proxy.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core-local includes */
#include <core_env.h>
#include <signal_transmitter.h>

using namespace Genode;

void Genode::init_core_signal_transmitter(Rpc_entrypoint &) { }

Rpc_entrypoint &Core_env::signal_ep() { return _entrypoint; }
