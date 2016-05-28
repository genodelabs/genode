/*
 * \brief  Signal-source capability type
 * \author Norman Feske
 * \date   2016-01-04
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SOURCE__CAPABILITY_H_
#define _INCLUDE__SIGNAL_SOURCE__CAPABILITY_H_

#include <base/capability.h>
#include <signal_source/signal_source.h>

namespace Genode { typedef Capability<Signal_source> Signal_source_capability; }

#endif /* _INCLUDE__SIGNAL_SOURCE__CAPABILITY_H_ */
