/*
 * \brief   TrustZone specific functions
 * \author  Stefan Kalkowski
 * \date    2012-10-10
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__TRUSTZONE_H_
#define _CORE__INCLUDE__TRUSTZONE_H_

namespace Kernel {

	class Pic;


	void init_trustzone(Pic * pic);
}

#endif /* _CORE__INCLUDE__TRUSTZONE_H_ */
