/*
 * \brief  Type used to store kernel-specific exception state
 * \author Norman Feske
 * \date   2016-12-13
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__PAGER_OBJECT_EXCEPTION_STATE_H_
#define _CORE__INCLUDE__PAGER_OBJECT_EXCEPTION_STATE_H_

#include <base/thread_state.h>

/* core includes */
#include <types.h>

namespace Core { using Pager_object_exception_state = Thread_state; }

#endif /* _CORE__INCLUDE__PAGER_OBJECT_EXCEPTION_STATE_H_ */
