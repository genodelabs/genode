/*
 * \brief  Regulator session capability type
 * \author Stefan Kalkowski
 * \date   2013-06-13
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__CAPABILITY_H_
#define _INCLUDE__REGULATOR_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <regulator_session/regulator_session.h>

namespace Regulator { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__REGULATOR_SESSION__CAPABILITY_H_ */
