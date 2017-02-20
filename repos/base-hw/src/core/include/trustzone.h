/*
 * \brief   TrustZone specific functions
 * \author  Stefan Kalkowski
 * \date    2012-10-10
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__TRUSTZONE_H_
#define _CORE__INCLUDE__TRUSTZONE_H_

/* core includes */
#include <pic.h>

namespace Kernel {
	void init_trustzone(Pic & pic);
}

#endif /* _CORE__INCLUDE__TRUSTZONE_H_ */
