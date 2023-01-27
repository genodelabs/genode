/*
 * \brief  Dummy device interface for virt_linux
 * \author Martin Stein
 * \author Christian Helmuth
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_KIT__DEVICE_H_
#define _LX_KIT__DEVICE_H_

#include <base/env.h>
#include <base/heap.h>

namespace Platform { struct Connection;  }
namespace Lx_kit   { struct Device_list; }

struct Platform::Connection
{
	Connection(Genode::Env &) {}
};


struct Lx_kit::Device_list
{
	Device_list(Genode::Entrypoint   &,
	            Genode::Heap         &,
	            Platform::Connection &) {}

	void update() {}

	template <typename FN>
	void for_each(FN const &) {}
};

#endif /* _LX_KIT__DEVICE_H_ */
