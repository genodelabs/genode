/*
 * \brief  Support code for the thread API
 * \author Martin Stein
 * \date   2013-05-07
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <rm_session/rm_session.h>

using namespace Genode;

/**
 * Return single instance of the context-area RM-session
 *
 * In base-hw core this object is never used because contexts
 * get allocated through the phys-mem allocator. Anyways the
 * accessor must exist because generic main-thread startup calls
 * it to ensure that common allocations do not steal context area.
 */
namespace Genode { Rm_session * env_context_area_rm_session() { return 0; } }

