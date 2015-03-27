/*
 * \brief  Sync-session capability type
 * \author Martin Stein
 * \date   2015-04-07
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SYNC_SESSION__CAPABILITY_H_
#define _SYNC_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>

/* local includes */
#include <sync_session/sync_session.h>

namespace Sync
{
	using Genode::Capability;

	typedef Capability<Session> Session_capability;
}

#endif /* _SYNC_SESSION__CAPABILITY_H_ */
