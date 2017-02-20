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

#ifndef _QSIGNALHANDLERTHREAD_H_
#define _QSIGNALHANDLERTHREAD_H_

/* Genode includes */
#include <base/signal.h>

/* Qt includes */
#include <QThread>

class QSignalHandlerThread : public QThread
{
	Q_OBJECT

	private:

		Genode::Signal_receiver &_signal_receiver;

	public:

		QSignalHandlerThread(Genode::Signal_receiver &signal_receiver)
		: _signal_receiver(signal_receiver) { }

		void run() override;
};

#endif /* _QSIGNALHANDLERTHREAD_H_ */
