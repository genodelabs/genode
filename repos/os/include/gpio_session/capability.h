/*
 * \brief  Gpio session capability type
 * \author Ivan Loskutov <ivan.loskutov@ksyslabs.org>
 * \date   2012-06-23
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPIO_SESSION__CAPABILITY_H_
#define _INCLUDE__GPIO_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <gpio_session/gpio_session.h>

namespace Gpio { using Session_capability = Genode::Capability<Session>; }

#endif /* _INCLUDE__GPIO_SESSION__CAPABILITY_H_ */
