/*
 * \brief  Copy a platform-capability to another protection domain.
 * \author Stefan Kalkowski
 * \date   2012-03-09
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <util/string.h>
#include <base/native_capability.h>

using namespace Genode;

void Cap_dst_policy::copy(void* dst, Native_capability_tpl<Cap_dst_policy>* src) {
	memcpy(dst, src, sizeof(Native_capability_tpl<Cap_dst_policy>)); }
