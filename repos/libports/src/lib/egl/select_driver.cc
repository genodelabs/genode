/*
 * \brief  Probe GPU device to select the proper Gallium3D driver
 * \author Norman Feske
 * \date   2010-09-23
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "select_driver.h"

const char *probe_gpu_and_select_driver()
{
	const char *result = 0;
	/* no support, currently we have no driver available */
	return result;
}
