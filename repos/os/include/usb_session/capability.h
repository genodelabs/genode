/**
 * \brief  USB session capability
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2014-12-08
 */

/*
 * Copyright (C) 2014-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */
#ifndef _INCLUDE__USB_SESSION__CAPABILITY_H_
#define _INCLUDE__USB_SESSION__CAPABILITY_H_

#include <base/capability.h>

namespace Usb {
	struct  Interface_session;
	struct  Device_session;
	class   Session;

	using Interface_capability = Genode::Capability<Interface_session>;
	using Device_capability    = Genode::Capability<Device_session>;
	using Session_capability   = Genode::Capability<Session>;
}

#endif /* _INCLUDE__USB_SESSION__CAPABILITY_H_ */
