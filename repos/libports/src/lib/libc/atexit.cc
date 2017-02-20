/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \date   2009-07-20
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern void genode_atexit(void (*func)(void));
extern "C" void atexit(void (*func)(void))
{
	genode_atexit(func);
}

extern void genode___cxa_finalize(void *dso);
extern "C" void __cxa_finalize(void *dso)
{
	genode___cxa_finalize(dso);
}
