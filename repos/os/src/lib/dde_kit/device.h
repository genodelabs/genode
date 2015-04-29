/*
 * \brief   Lib internal interface to use and request resources provided by
 *          platform driver
 * \author  Alexander Boettcher
 * \date    2015-04-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

#include <io_port_session/capability.h>

namespace Dde_kit { class Device; }

class Dde_kit::Device {

	public:
		static Genode::Io_port_session_capability io_port(int bus, int dev,
		                                                  int fun,
		                                                  unsigned short bda);
};
