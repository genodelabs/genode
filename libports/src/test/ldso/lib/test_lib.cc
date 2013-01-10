/*
 * \brief  Test for cross library linking
 * \author Sebastian Sumpf
 * \date   2011-07-20
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <base/printf.h>
#include "test-ldso.h"

void Link::cross_lib_exception() { throw 668; }
