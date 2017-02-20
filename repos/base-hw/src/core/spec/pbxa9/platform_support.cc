/*
 * \brief   Parts of platform that are specific to PBXA9
 * \author  Martin Stein
 * \date    2012-04-27
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <platform.h>
#include <board.h>
#include <cpu.h>
#include <pic.h>

/* base-internal includes */
#include <base/internal/unmanaged_singleton.h>

using namespace Genode;


Genode::Arm::User_context::User_context() { cpsr = Psr::init_user(); }
