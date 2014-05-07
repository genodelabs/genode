/*
 * \brief  Test if global static constructors in host shared libs get called
 * \author Christian Prochaska
 * \date   2011-11-24
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

struct Testlib_testclass
{
	Testlib_testclass();
	void dummy();
};
