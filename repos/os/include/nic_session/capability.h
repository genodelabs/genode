/*
 * \brief  NIC session capability type
 * \author Norman Feske
 * \date   2009-11-13
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC_SESSION__CAPABILITY_H_
#define _INCLUDE__NIC_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <nic_session/nic_session.h>

namespace Nic { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__NIC_SESSION__CAPABILITY_H_ */
