/*
 * \brief  GPU session capability type
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_SESSION__CAPABILITY_H_
#define _INCLUDE__GPU_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <gpu_session/gpu_session.h>

namespace Gpu { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__GPU_SESSION__CAPABILITY_H_ */
