/*
 * \brief  Interrupt handler interface
 * \author Christian Prochaska
 * \date   2013-10-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__INTERRUPT_HANDLER__H_
#define _NOUX__INTERRUPT_HANDLER__H_

/* Genode includes */
#include <util/list.h>

namespace Noux {

	struct Interrupt_handler : List<Interrupt_handler>::Element
	{
		virtual void handle_interrupt() = 0;
	};
};

#endif /* _NOUX__INTERRUPT_HANDLER__H_ */

