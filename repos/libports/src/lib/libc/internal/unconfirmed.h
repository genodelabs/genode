/*
 * \brief  Utility to automatically unroll unconfirmed operations
 * \author Christian Helmuth
 * \date   2020-07-31
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__UNCONFIRMED_H_
#define _LIBC__INTERNAL__UNCONFIRMED_H_

namespace Libc {
	template <typename> struct Unconfirmed;

	template <typename FUNC>
	Unconfirmed<FUNC> make_unconfirmed(FUNC const &cleanup)
	{
		return { cleanup };
	}
};


template <typename FUNC>
struct Libc::Unconfirmed
{
	bool confirmed { false };

	FUNC const &cleanup;

	Unconfirmed(FUNC const &cleanup) : cleanup(cleanup) { }
	~Unconfirmed() { if (!confirmed) cleanup(); }

	void confirm() { confirmed = true; }
};

#endif /* _LIBC__INTERNAL__UNCONFIRMED_H_ */
