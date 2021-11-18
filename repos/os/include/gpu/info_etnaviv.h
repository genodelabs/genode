/*
 * \brief  Gpu Information ETNAVIV
 * \author Josef Soentgen
 * \date   2021-09-20
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_INFO_ETNAVIV_H_
#define _INCLUDE__GPU_INFO_ETNAVIV_H_

#include <base/output.h>
#include <session/session.h>
#include <gpu_session/gpu_session.h>

namespace Gpu {

	struct Info_etnaviv;
} /* namespace Gpu */


/*
 * Gpu information
 *
 * Used to query information in the DRM backend
 */
struct Gpu::Info_etnaviv
{
	/*
	 * Size the array based on the list of params in
	 * etnaviv_drm.h that allow for 1:1 access.
	 */
	enum { MAX_ETNAVIV_PARAMS = 32, };
	using Param = Genode::uint64_t;
	Param param[MAX_ETNAVIV_PARAMS] { };
};

#endif /* _INCLUDE__GPU_INFO_ETNAVIV_H_ */
