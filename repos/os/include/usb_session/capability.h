/**
 * \brief  USB session capability
 * \author Sebastian Sumpf
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__CAPABILITY_H_
#define _INCLUDE__USB_SESSION__CAPABILITY_H_

#include <base/capability.h>

namespace Usb {
	class Session;
	typedef Genode::Capability<Session> Session_capability;
}

#endif /* _INCLUDE__USB_SESSION__CAPABILITY_H_ */
