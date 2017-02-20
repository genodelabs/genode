/*
 * \brief  Dummy stubs for network-related Noux functions
 * \author Norman Feske
 * \date   2012-05-24
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <child.h>

void init_network() { }

bool Noux::Child::_syscall_net(Noux::Session::Syscall sc) { return false; }
