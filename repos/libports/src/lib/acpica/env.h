/*
 * \brief  Genode environment for ACPICA library
 * \author Christian Helmuth
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _ACPICA__ENV_H_
#define _ACPICA__ENV_H_

#include <base/env.h>
#include <base/allocator.h>
#include <platform_session/client.h>

namespace Acpica {
	Genode::Env       & env();
	Genode::Allocator & heap();
	Platform::Client  & platform();
	bool                platform_drv();
}

#endif /* _ACPICA__ENV_H_ */
