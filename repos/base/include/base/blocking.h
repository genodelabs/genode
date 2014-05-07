/*
 * \brief  Support for blocking operations
 * \author Norman Feske
 * \date   2007-09-06
 *
 * In Genode, two operations may block a thread,
 * waiting at a lock or performing an IPC call.
 * Both operations may be canceled when killing
 * the thread. In this case, the thread unblocks
 * and throws an exception, and therefore, is able
 * to clean up the thread state before exiting.
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__BLOCKING_H_
#define _INCLUDE__BASE__BLOCKING_H_

#include <base/exception.h>

namespace Genode {  class Blocking_canceled : public Exception { }; }

#endif /* _INCLUDE__BASE__BLOCKING_H_ */
