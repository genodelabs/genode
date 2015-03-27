/*
 * \brief  Abstract platform device interface
 * \author Alexander Boettcher
 * \date   2015-03-15
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#pragma once

#include <irq_session/capability.h>

namespace Platform { class Device; }

struct Platform::Device
{
	/**
	 * Get IRQ session capability
	 */
	virtual	Genode::Irq_session_capability irq(Genode::uint8_t) = 0;
};
