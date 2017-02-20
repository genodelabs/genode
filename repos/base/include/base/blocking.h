/*
 * \brief  Support for blocking operations
 * \author Norman Feske
 * \date   2007-09-06
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__BLOCKING_H_
#define _INCLUDE__BASE__BLOCKING_H_

#include <base/exception.h>

namespace Genode { class Blocking_canceled; }

/**
 * Blocking-canceled exception
 *
 * Two operations may block a thread, waiting at a lock or performing an RPC
 * call. Both operations may be canceled when the thread is destructed. In this
 * case, the thread unblocks and throws an exception, and therefore, is able to
 * clean up the thread state before exiting.
 */
class Genode::Blocking_canceled : public Exception { };

#endif /* _INCLUDE__BASE__BLOCKING_H_ */
