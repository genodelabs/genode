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
