/*
 * \brief  Menu-view dialog handling
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DIALOG_H_
#define _VIEW__DIALOG_H_

/* Genode includes */
#include <input/event.h>

/* local includes */
#include "types.h"

namespace Sculpt { struct Dialog; }


struct Sculpt::Dialog : Interface
{
	/**
	 * Interface for triggering the (re-)generation of a menu-view dialog
	 *
	 * This interface ls implemented by a top-level dialog and called by a sub
	 * dialog.
	 */
	struct Generator : Interface { virtual void generate_dialog() = 0; };

	virtual void hover(Xml_node hover) = 0;
};

#endif /* _VIEW__DIALOG_H_ */
