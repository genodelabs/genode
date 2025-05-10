/*
 * \brief  Revocation of soignal contexts
 * \author Stefan Kalkowski
 * \date   2025-05-10
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__REVOKE_H_
#define _CORE__REVOKE_H_

#include <base/signal.h>
#include <util/interface.h>

namespace Core { struct Revoke; }

struct Core::Revoke : Interface
{
	virtual void revoke_signal_context(Signal_context_capability) = 0;
};

#endif /* _CORE__REVOKE_H_ */
