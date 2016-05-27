/*
 * \brief  Debug utilities
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2014-10-10
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LX_KIT__INTERNAL__DEBUG_H_
#define _LX_KIT__INTERNAL__DEBUG_H_

#ifndef PDBGV
#define PDBGV(...) do { if (verbose) PDBG(__VA_ARGS__); } while (0)
#endif

#endif /* _LX_KIT__INTERNAL__DEBUG_H_ */
