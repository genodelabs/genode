/*
 * \brief  L4ka::Pistachio system-call bindings
 * \author Norman Feske
 * \date   2021-02-07
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BASE__INTERNAL__PISTACHIO_H_
#define _BASE__INTERNAL__PISTACHIO_H_

namespace Pistachio {
#include <l4/types.h>
#include <l4/thread.h>
#include <l4/message.h>
#include <l4/ipc.h>
#include <l4/schedule.h>
#include <l4/kdebug.h>
#include <l4/kip.h>
#include <l4/space.h>
#include <l4/sigma0.h>
#include <l4/arch.h>
#include <l4/bootinfo.h>
#include <l4/misc.h>
}

#endif /* _BASE__INTERNAL__PISTACHIO_H_ */
