/*
 * \brief  Uplink session capability type
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPLINK_SESSION__CAPABILITY_H_
#define _UPLINK_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>
#include <uplink_session/uplink_session.h>

namespace Uplink {

	typedef Genode::Capability<Session> Session_capability;
}

#endif /* _UPLINK_SESSION__CAPABILITY_H_ */
