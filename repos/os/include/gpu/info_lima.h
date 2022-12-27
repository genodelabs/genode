/*
 * \brief  Gpu Information Lima
 * \author Josef Soentgen
 * \date   2022-06-14
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_INFO_LIMA_H_
#define _INCLUDE__GPU_INFO_LIMA_H_

#include <base/output.h>
#include <session/session.h>
#include <gpu_session/gpu_session.h>

namespace Gpu {

	struct Info_lima;
} /* namespace Gpu */


/*
 * Gpu information
 *
 * Used to query information in the DRM backend
 */
struct Gpu::Info_lima
{
	/*
	 * Size the array based on the list of params in
	 * lima_drm.h that allow for 1:1 access.
	 */
	enum { MAX_LIMA_PARAMS = 4, };
	using Param = Genode::uint64_t;
	Param param[MAX_LIMA_PARAMS] { };
};

#endif /* _INCLUDE__GPU_INFO_LIMA_H_ */
