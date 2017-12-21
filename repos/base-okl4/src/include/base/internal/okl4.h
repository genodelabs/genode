/*
 * \brief  OKL4 system-call bindings
 * \author Norman Feske
 * \date   2017-12-22
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BASE__INTERNAL__OKL4_H_
#define _BASE__INTERNAL__OKL4_H_

/* OKL4 includes */
namespace Okl4 { extern "C" {
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <l4/config.h>
#include <l4/types.h>
#include <l4/ipc.h>
#include <l4/utcb.h>
#include <l4/thread.h>
#include <l4/kdebug.h>
#include <l4/schedule.h>
#include <l4/space.h>
#include <l4/map.h>
#include <l4/interrupt.h>
#include <l4/security.h>
#pragma GCC diagnostic pop
#undef UTCB_SIZE
} }

#endif /* _BASE__INTERNAL__OKL4_H_ */
