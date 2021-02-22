/*
 * \brief  I2C session capability type
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-02-26
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__I2C_SESSION__CAPABILITY_H_
#define _INCLUDE__I2C_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <i2c_session/i2c_session.h>

namespace I2c { typedef Genode::Capability<Session> Session_capability; }

#endif /*_INCLUDE__I2C_SESSION__CAPABILITY_H_ */
