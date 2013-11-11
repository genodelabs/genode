/*
 * \brief   Priority definition for scheduling
 * \author  Stefan Kalkowski
 * \date    2013-11-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _KERNEL__PRIORITY_H_
#define _KERNEL__PRIORITY_H_

#include <util/misc_math.h>

namespace Kernel
{
	class Priority;
}


class Kernel::Priority
{
	private:

		unsigned _prio;

	public:

		enum { MAX = 255 };

		Priority(unsigned prio = MAX) : _prio(Genode::min(prio, MAX)) { }

		Priority &operator=(unsigned const &prio)
		{
			_prio = Genode::min(prio, MAX);
			return *this;
		}

		operator unsigned() const { return _prio; }
};

#endif /* _KERNEL__PRIORITY_H_ */
