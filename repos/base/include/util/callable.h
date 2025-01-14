/*
 * \brief  Utility for passing lambda arguments to non-template functions
 * \author Norman Feske
 * \date   2025-01-14
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__CALLABLE_H_
#define _INCLUDE__UTIL__CALLABLE_H_

#include <util/interface.h>

namespace Genode { template <typename, typename...> struct Callable; };


/**
 * Utility for passing lambda arguments to non-template functions
 *
 * A function or method taking a lambda as argument must always be a template
 * because the captured state is known only at the caller site. For example,
 * within a display driver, the following function is meant to call 'fn' for
 * each 'Connector const &' that is currently known.
 *
 * ! void for_each_connector(auto const &fn);
 *
 * The type of 'fn' as template parameter stands in the way in two situations.
 * First, 'for_each_connector' must be implemented in a header because it is a
 * template. But its inner working might be complex and better be hidden inside
 * a compilation unit. Second, 'for_each_connector' cannot be part of an
 * abstract interface because template methods cannot be virtual functions.
 *
 * The 'Callable' utility addresses both situations by introducing an abstract
 * function type 'Ft' for the plain signature (arguments, return type) of a
 * lambda, and an 'Fn' type implementing the abstract function for a concrete
 * lambda argument. E.g., the following 'With_connector' type defines a
 * signature for a lambda that takes a 'Connector const &' argument.
 *
 * ! With_connector = Callable<void, Connector const &>;
 *
 * The 'With_connector' definition now allows for defining a pure virtual
 * function by referring to the signature's function type 'Ft'. It is a good
 * practice to use a '_' prefix because this method is not meant to be the API.
 *
 * ! virtual void _for_each_connector(With_connector::Ft const &) = 0;
 *
 * The user-facing API should best accept a regular 'auto const &fn' argument
 * and call the virtual function with a concrete implementation of the function
 * type, which is accomplished via the 'Fn' type.
 *
 * ! void for_each_connector(auto const &fn)
 * ! {
 * !   _for_each_connector( With_connector::Fn { fn } );
 * ! }
 *
 * At the implementation site of '_for_each_connector', the 'Ft' argument
 * can be called like a regular lambda argument. At the caller site,
 * 'for_each_connector' accepts a regular lambda argument naturally expecting
 * a 'Connector const &'.
 */
template <typename RET, typename... ARGS>
struct Genode::Callable
{
	struct Ft : Interface { virtual RET operator () (ARGS &&...) const = 0; };

	template <typename FN>
	struct Fn : Ft
	{
		FN const &_fn;
		Fn(FN const &fn) : _fn(fn) { };
		RET operator () (ARGS &&... args) const override { return _fn(args...); }
	};
};


template <typename... ARGS>
struct Genode::Callable<void, ARGS...>
{
	struct Ft : Interface { virtual void operator () (ARGS &&...) const = 0; };

	template <typename FN>
	struct Fn : Ft
	{
		FN const &_fn;
		Fn(FN const &fn) : _fn(fn) { };
		void operator () (ARGS &&... args) const override { _fn(args...); }
	};
};

#endif /* _INCLUDE__UTIL__CALLABLE_H_ */
