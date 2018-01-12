/*
 * \brief  Hook for kernel-specific child-handling policy
 * \author Norman Feske
 * \date   2018-01-30
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__INTERNAL__CHILD_POLICY_H_
#define _INCLUDE__BASE__INTERNAL__CHILD_POLICY_H_

namespace Genode { constexpr bool KERNEL_SUPPORTS_EAGER_CHILD_DESTRUCTION = true; }

#endif /* _INCLUDE__BASE__INTERNAL__CHILD_POLICY_H_ */
