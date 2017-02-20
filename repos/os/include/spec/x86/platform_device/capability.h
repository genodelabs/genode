/*
 * \brief  PCI device capability type
 * \author Norman Feske
 * \date   2008-08-16
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/capability.h>
#include <platform_device/platform_device.h>

namespace Platform { typedef Genode::Capability<Device> Device_capability; }
