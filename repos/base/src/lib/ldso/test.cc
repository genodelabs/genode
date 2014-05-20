/*
 * \brief  Internal test functions
 * \author Sebastian Sumpf
 * \date   2011-08-25
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <base/exception.h>

void __ldso_raise_exception()
{
	throw Genode::Exception();
}
