/*
 * \brief  Platform session capability type
 * \author Stefan Kalkowski
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PLATFORM_SESSION__CAPABILITY_H_
#define _INCLUDE__PLATFORM_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <platform_session/platform_session.h>

namespace Platform { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__PLATFORM_SESSION__CAPABILITY_H_ */
