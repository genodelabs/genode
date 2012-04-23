/*
 * \brief  Noux-session capability type
 * \author Norman Feske
 * \date   2011-02-15
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NOUX_SESSION__CAPABILITY_H_
#define _INCLUDE__NOUX_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <noux_session/noux_session.h>

namespace Noux { typedef Capability<Session> Session_capability; }

#endif /* _INCLUDE__NOUX_SESSION__CAPABILITY_H_ */
