/*
 * \brief  Loader-session capability type
 * \author Christian Prochaska
 * \date   2009-10-05
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__LOADER_SESSION__CAPABILITY_H_
#define _INCLUDE__LOADER_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <loader_session/loader_session.h>

namespace Loader { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__LOADER_SESSION__CAPABILITY_H_ */
