/*
 * \brief  Zynq Gpio session capability type
 * \author Mark Albers
 * \date   2015-04-02
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__GPIO_SESSION__CAPABILITY_H_
#define _INCLUDE__GPIO_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <gpio_session/zynq/gpio_session.h>

namespace Gpio { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__GPIO_SESSION__CAPABILITY_H_ */
