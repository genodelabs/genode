/*
 * \brief  C-library back end
 * \author Christian Prochaska
 * \date   2010-05-16
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>

extern "C" const char *gai_strerror(int errcode)
{
	static const char *result = "gai_strerror called, not yet implemented!";
	Genode::log(__func__, ": ", result);
	return result;
}
