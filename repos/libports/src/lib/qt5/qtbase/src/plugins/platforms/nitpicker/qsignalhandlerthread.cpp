/*
 * \brief  QPA signal handler thread
 * \author Christian Prochaska
 * \date   2015-09-18
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "qsignalhandlerthread.h"

void QSignalHandlerThread::run()
{
	for (;;) {
		Genode::Signal s = _signal_receiver.wait_for_signal();
		static_cast<Genode::Signal_dispatcher_base*>(s.context())->dispatch(s.num());
	}
}
