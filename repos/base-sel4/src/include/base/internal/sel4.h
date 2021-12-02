/*
 * \brief  seL4 kernel bindings
 * \author Norman Feske
 * \date   2021-12-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__SEL4_H_
#define _INCLUDE__BASE__INTERNAL__SEL4_H_

/*
 * Disable -Wconversion warnings caused by seL4 headers
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include <sel4/sel4.h>
#pragma GCC diagnostic pop

#endif /* _INCLUDE__BASE__INTERNAL__SEL4_H_ */
