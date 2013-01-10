/*
 * \brief  ldso test program
 * \author Sebastian Sumpf
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE_RTLD_RTLD_LIB_H
#define _INCLUDE_RTLD_RTLD_LIB_H

namespace Link {
	void dynamic_link_test();
	void static_function_object();
	void cross_lib_exception();
	void raise_exception();
}

#endif
