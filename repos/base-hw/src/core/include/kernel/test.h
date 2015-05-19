/*
 * \brief   Automated testing of kernel internals
 * \author  Stefan Kalkowski
 * \author  Martin Stein
 * \date    2015-05-21
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__TEST_H_
#define _KERNEL__TEST_H_

namespace Kernel {

	/**
	 * Hook that enables automated testing of kernel internals
	 */
	void test();
}

#endif /* _KERNEL__TEST_H_ */
