/*
 * \brief  Utility for attempting VirtualBox operations
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-12-11
 *
 * The utility avoids repetitive code for checking the return value of
 * VirtualBox API functions that are expected to always succeed.
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _ATTEMPT_H_
#define _ATTEMPT_H_

#include <base/exception.h>


class Fatal : Genode::Exception { };


template <typename FN, typename... ERR_MSG>
static void attempt(FN const &fn, ERR_MSG &&... err_msg)
{
	HRESULT const rc = fn();

	if (FAILED(rc)) {
		Genode::error(err_msg..., " (rc=", rc, ")");
		throw Fatal();
	}
}

#endif /* _ATTEMPT_H_ */
