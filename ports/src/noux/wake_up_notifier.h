/*
 * \brief  Utility for implementing blocking syscalls
 * \author Norman Feske
 * \date   2011-11-05
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__WAKE_UP_NOTIFIER__H_
#define _NOUX__WAKE_UP_NOTIFIER__H_

/* Genode includes */
#include <util/list.h>
#include <base/semaphore.h>

namespace Noux {

	struct Wake_up_notifier : List<Wake_up_notifier>::Element
	{
		Semaphore *semaphore;

		Wake_up_notifier(Semaphore *semaphore = 0)
		: semaphore(semaphore) { }

		void wake_up()
		{
			if (semaphore)
				semaphore->up();
		}
	};
};

#endif /* _NOUX__WAKE_UP_NOTIFIER__H_ */
