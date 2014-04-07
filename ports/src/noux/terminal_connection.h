/*
 * \brief  Terminal connection
 * \author Josef Soentgen
 * \date   2012-08-02
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__TERMINAL_CONNECTION_H_
#define _NOUX__TERMINAL_CONNECTION_H_

/* Genode includes */
#include <base/printf.h>
#include <base/lock.h>
#include <terminal_session/connection.h>
#include <util/string.h>

/* Noux includes */
#include <noux_session/sysio.h>
#include <vfs/file_system.h>


namespace Noux { Terminal::Connection *terminal(); }

#endif /* _NOUX__TERMINAL_CONNECTION_H_ */
