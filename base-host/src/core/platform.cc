/*
 * \brief   Platform interface implementation
 * \author  Norman Feske
 * \date    2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>

/* core includes */
#include <core_parent.h>
#include <platform.h>

using namespace Genode;


Platform::Platform()
{
	PWRN("not implemented");
}


/********************************
 ** Generic platform interface **
 ********************************/

void Platform::wait_for_exit()
{
	sleep_forever();
}


void Core_parent::exit(int exit_value) { }
