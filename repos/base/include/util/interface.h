/*
 * \brief  Common base class of abstract base classes
 * \author Norman Feske
 * \date   2017-12-21
 *
 * The 'Interface' class relieves abstract base classes from manually
 * implementing a virtual destructor.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__INTERFACE_H_
#define _INCLUDE__UTIL__INTERFACE_H_

namespace Genode { struct Interface { virtual ~Interface() { }; }; }

#endif /* _INCLUDE__UTIL__INTERFACE_H_ */
