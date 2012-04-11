/*
 * \brief  File-system session capability type
 * \author Norman Feske
 * \date   2012-04-05
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__FILE_SYSTEM_SESSION__CAPABILITY_H_
#define _INCLUDE__FILE_SYSTEM_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <file_system_session/file_system_session.h>

namespace File_system { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__FILE_SYSTEM_SESSION__CAPABILITY_H_ */
