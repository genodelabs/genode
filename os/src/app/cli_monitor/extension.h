/*
 * \brief  Interface for platform-specific CLI-monitor commands
 * \author Norman Feske
 * \date   2013-03-21
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _EXTENSION_H_
#define _EXTENSION_H_

/* local includes */
#include <line_editor.h>

/**
 * Initialize and register platform-specific commands
 */
void init_extension(Command_registry &);

#endif /* _EXTENSION_H_ */
