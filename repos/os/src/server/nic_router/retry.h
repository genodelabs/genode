/*
 * \brief  Utility to execute a function repeatedly
 * \author Martin Stein
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RETRY_H_
#define _RETRY_H_

namespace Net {

	template <typename EXCEPTION_1, typename EXCEPTION_2>
	void retry_once(auto const &attempt_fn, auto const &exception_fn, auto const &failed_fn)
	{
		for (unsigned i = 0; ; i++) {
			try {
				attempt_fn();
				return;
			}
			catch (EXCEPTION_1) { }
			catch (EXCEPTION_2) { }
			if (i == 1) {
				failed_fn();
				return;
			}
			exception_fn();
		}
	}
}

#endif /* _RETRY_H_ */
