/*
 * \brief  VMM GUI session capability type
 * \author Stefan Kalkowski
 * \date   2013-04-17
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VMM_GUI_SESSION__CAPABILITY_H_
#define _INCLUDE__VMM_GUI_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>
#include <vmm_gui_session/vmm_gui_session.h>

namespace Vmm_gui { typedef Genode::Capability<Session> Capability; }

#endif /* _INCLUDE__VMM_GUI_SESSION__CAPABILITY_H_ */
