/*
 * \brief  VM-session capability type
 * \author Stefan Kalkowski
 * \date   2012-10-02
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VM_SESSION__CAPABILITY_H_
#define _INCLUDE__VM_SESSION__CAPABILITY_H_

/* Genode includes */
#include <base/capability.h>
#include <vm_session/vm_session.h>

namespace Genode { typedef Capability<Vm_session> Vm_session_capability; }

#endif /* _INCLUDE__VM_SESSION__CAPABILITY_H_ */
