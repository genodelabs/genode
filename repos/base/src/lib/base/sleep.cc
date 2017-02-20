/*
 * \brief  Lay back and relax
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/sleep.h>
#include <base/lock.h>

void Genode::sleep_forever()
{
	Lock sleep;
	while (true) sleep.lock();
}
