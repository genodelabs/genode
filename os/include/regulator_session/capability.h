/*
 * \brief  regulator driver session capability type
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org> 
 * \date   2012-02-15
 */

/*
 * Copyright (C) 2012 Ksys Labs LLC
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__REGULATOR_SESSION__CAPABILITY_H_
#define _INCLUDE__REGULATOR_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <regulator_session/regulator_session.h>

namespace Regulator { typedef Genode::Capability<Session> Session_capability; }

#endif /* _INCLUDE__REGULATOR_SESSION__CAPABILITY_H_ */
