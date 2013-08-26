/*
 * \brief  Global environment
 * \author Stefan Kalkowski
 * \date   2013-05-23
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "env.h"

Genode::Signal_receiver* Net::Env::receiver()
{
	static Genode::Signal_receiver receiver;
	return &receiver;
}
