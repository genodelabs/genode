/*
 * \brief  UTCB definition
 * \author Norman Feske
 * \date   2016-03-08
 *
 * On this kernel, UTCBs are not placed within the the stack area. Each
 * thread can request its own UTCB pointer using the kernel interface.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_
#define _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_

namespace Genode { struct Native_utcb { }; }

#endif /* _INCLUDE__BASE__INTERNAL__NATIVE_UTCB_H_ */

