/*
 * \brief  Dummy stub for trace driver
 * \author Norman
 * \date   2010-07-12
 *
 * The Intel driver contains a hard-coded initialization of the trace driver.
 * With the dummy stub, we avoid having to use the trace driver. Using the
 * trace driver would implicate the need for a working pthreads implementation,
 * which we don't have yet.
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* includes from 'gallium/drivers/trace' */
#include <tr_drm.h>

struct drm_api *trace_drm_create(struct drm_api *api)
{
	return api;
}

