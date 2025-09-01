/*
 * \brief  Genode Node plus local utilities
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NODE_H_
#define _NODE_H_

/* Genode includes */
#include <base/node.h>
#include <base/duration.h>


namespace Genode {

	Microseconds read_sec_attr(Node     const &node,
	                           char     const *name,
	                           uint64_t const  default_sec);
}

#endif /* _NODE_H_ */
