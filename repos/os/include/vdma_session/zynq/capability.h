/*
 * \brief  Zynq VDMA session capability type
 * \author Mark Albers
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VDMA_SESSION__CAPABILITY_H_
#define _INCLUDE__VDMA_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <vdma_session/zynq/vdma_session.h>

namespace Vdma { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__VDMA_SESSION__CAPABILITY_H_ */
