/*
 * \brief  CAP-session interface
 * \author Norman Feske
 * \date   2006-06-23
 *
 * \deprecated
 *
 * This header is scheduled for removal. It exists for API compatiblity only.
 */

#ifndef INCLUDED_BY_ENTRYPOINT_CC
#warning cap_session/cap_session.h is deprecated
#endif

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__CAP_SESSION__CAP_SESSION_H_
#define _INCLUDE__CAP_SESSION__CAP_SESSION_H_

#include <pd_session/pd_session.h>

namespace Genode { typedef Pd_session Cap_session; }

#endif /* _INCLUDE__CAP_SESSION__CAP_SESSION_H_ */
