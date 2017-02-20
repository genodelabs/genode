/*
 * \brief  Base of each routing rule
 * \author Martin Stein
 * \date   2016-08-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RULE_H_
#define _RULE_H_

/* Genode includes */
#include <base/exception.h>

namespace Net { struct Rule { struct Invalid : Genode::Exception { }; }; }

#endif /* _RULE_H_ */
