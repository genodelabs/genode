/*
 * \brief  Genode C API string API
 * \author Stefan Kalkowski
 * \date   2013-06-26
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/string.h>

namespace Fiasco {
#include <genode/string.h>
}
using namespace Fiasco;


extern "C" void genode_memcpy(void *dst, void *src, unsigned long size)
{
	Genode::memcpy(dst, src, size);
}
