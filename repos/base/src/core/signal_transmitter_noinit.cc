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
#include <signal_transmitter.h>

void Genode::init_core_signal_transmitter(Rpc_entrypoint &) { }
