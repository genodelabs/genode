/*
 * \brief  Interrupt handler interface
 * \author Christian Prochaska
 * \date   2013-10-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__INTERRUPT_HANDLER__H_
#define _NOUX__INTERRUPT_HANDLER__H_

#include <noux_session/sysio.h>

namespace Noux {

	struct Interrupt_handler
	{
		virtual void handle_interrupt(Sysio::Signal) = 0;
	};
};

#endif /* _NOUX__INTERRUPT_HANDLER__H_ */

