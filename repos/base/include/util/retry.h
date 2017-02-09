/*
 * \brief  Utility to execute a function repeatedly
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2015-04-29
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__RETRY_H_
#define _INCLUDE__UTIL__RETRY_H_

namespace Genode {

	template <typename EXC, typename FUNC, typename HANDLER>
	auto retry(FUNC func, HANDLER handler,
	           unsigned attempts = ~0U) -> decltype(func());
}

/**
 * Repeatedly try to execute a function 'func'
 *
 * If the function 'func' throws an exception of type 'EXC', the 'handler'
 * is called and the function call is retried.
 *
 * \param EXC       exception type to handle
 * \param func      functor to execute
 * \param handler   exception handler executed if 'func' raised an exception
 *                  of type 'EXC'
 * \param attempts  number of attempts to execute 'func' before giving up
 *                  and reflecting the exception 'EXC' to the caller. If not
 *                  specified, attempt infinitely.
 */
template <typename EXC, typename FUNC, typename HANDLER>
auto Genode::retry(FUNC func, HANDLER handler,
                   unsigned attempts) -> decltype(func())
{
	for (unsigned i = 0; attempts == ~0U || i < attempts; i++)
		try { return func(); }
		catch (EXC) { handler(); }

	throw EXC();
}

#endif /* _INCLUDE__UTIL__RETRY_H_ */
