/*
 * \brief  Path handling utility for Noux
 * \author Norman Feske
 * \date   2011-02-17
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__PATH_H_
#define _NOUX__PATH_H_

/* Genode includes */
#include <os/path.h>

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux { typedef Path<Sysio::MAX_PATH_LEN> Absolute_path; }

#endif /* _NOUX__PATH_H_ */
