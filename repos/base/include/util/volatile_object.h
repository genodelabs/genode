/*
 * \brief  Utility for manual in-place construction of objects
 * \author Norman Feske
 * \date   2014-01-10
 *
 * \deprecated  use 'util/reconstructible.h' instead
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__VOLATILE_OBJECT_H_
#define _INCLUDE__UTIL__VOLATILE_OBJECT_H_

#include <util/reconstructible.h>

#warning "'util/volatile_object.h' is deprecated, use 'util/reconstructible.h' instead (see https://github.com/genodelabs/genode/issues/2151)"

namespace Genode {

	template <typename T>
	struct Volatile_object : Reconstructible<T>
	{
		using Reconstructible<T>::Reconstructible;
	};

	template <typename T>
	struct Lazy_volatile_object : Constructible<T>
	{
		using Constructible<T>::Constructible;
	};
}

#endif /* _INCLUDE__UTIL__VOLATILE_OBJECT_H_ */
