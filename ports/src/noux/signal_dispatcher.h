/*
 * \brief  Abstract interface for dispatching signals
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__SIGNAL_DISPATCHER_H_
#define _NOUX__SIGNAL_DISPATCHER_H_

/* Genode includes */
#include <base/signal.h>

namespace Noux {

	struct Signal_dispatcher : public Signal_context
	{
		virtual void dispatch() = 0;
	};
}

#endif /* _NOUX__SIGNAL_DISPATCHER_H_ */
