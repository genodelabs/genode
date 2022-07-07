/*
 * \brief  Libdrm ioctl back end dispatcher
 * \author Josef Soentgen
 * \date   2022-07-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#ifndef _INCLUDE__LIBDRM__IOCTL_DISPATCH_H_
#define _INCLUDE__LIBDRM__IOCTL_DISPATCH_H_

namespace Libdrm {
	enum Driver { INVALID, ETNAVIV, LIMA };
}; /* namespace Libdrm */

void drm_init(Libdrm::Driver);

#endif /* #ifndef _INCLUDE__LIBDRM__IOCTL_DISPATCH_H_ */
