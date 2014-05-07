/*
 * \brief  Signal-session capability type
 * \author Norman Feske
 * \date   2009-08-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__CAPABILITY_H_
#define _INCLUDE__SIGNAL_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <signal_session/signal_session.h>

namespace Genode {

	typedef Capability<Signal_session> Signal_session_capability;
}

#endif /* _INCLUDE__SIGNAL_SESSION__CAPABILITY_H_ */
