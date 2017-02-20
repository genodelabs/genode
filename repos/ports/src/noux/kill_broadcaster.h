/*
 * \brief  Kill_broadcaster interface
 * \author Christian Prochaska
 * \date   2014-01-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__KILL_BROADCASTER__H_
#define _NOUX__KILL_BROADCASTER__H_

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux {

	struct Kill_broadcaster
	{
		virtual bool kill(int pid, Noux::Sysio::Signal sig) = 0;
	};
};

#endif /* _NOUX__KILL_BROADCASTER__H_ */

