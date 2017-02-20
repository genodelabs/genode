/*
 * \brief  Nitpicker-session capability type
 * \author Norman Feske
 * \date   2008-08-16
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NITPICKER_SESSION__CAPABILITY_H_
#define _INCLUDE__NITPICKER_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <nitpicker_session/nitpicker_session.h>

namespace Nitpicker {

	typedef Genode::Capability<Session> Session_capability;
}

#endif /* _INCLUDE__NITPICKER_SESSION__CAPABILITY_H_ */
