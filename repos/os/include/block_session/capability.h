/*
 * \brief  Block session capability type
 * \author Stefan Kalkowski
 * \date   2010-07-07
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BLOCK_SESSION__CAPABILITY_H_
#define _INCLUDE__BLOCK_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <block_session/block_session.h>

namespace Block { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__BLOCK_SESSION__CAPABILITY_H_ */
