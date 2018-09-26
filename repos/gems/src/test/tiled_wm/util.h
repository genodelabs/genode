/*
 * \brief  Tiled-WM test: utilities
 * \author Christian Helmuth
 * \date   2018-09-26
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TEST__TILED_WM__UTIL_H_
#define _TEST__TILED_WM__UTIL_H_

/* Genode includes */
#include <base/signal.h>
#include <base/thread.h>
#include <base/log.h>
#include <util/string.h>

/* Qt includes */
#include <QApplication>
#include <QObject>
#include <QFile>
#include <QDebug>
#include <QFrame>

/* Qoost includes */
#include <qoost/qmember.h>

/* Libc includes */
#include <libc/component.h>


typedef Genode::String<32> Name;


/*
 * Genode signal to queued Qt signal proxy
 */
class Genode_signal_proxy : public QObject,
                            public Genode::Signal_dispatcher<Genode_signal_proxy>
{
	Q_OBJECT

	public:

		Genode_signal_proxy(Genode::Signal_receiver &sig_rec)
		:
			Genode::Signal_dispatcher<Genode_signal_proxy>(
				sig_rec, *this, &Genode_signal_proxy::handle_genode_signal)
		{
			connect(this, SIGNAL(internal_signal()),
			        this, SIGNAL(signal()),
			        Qt::QueuedConnection);
		}

	/* called by signal dispatcher / emits internal signal in context of
	 * signal-dispatcher thread */
	void handle_genode_signal(unsigned = 0) { Q_EMIT internal_signal(); }

	Q_SIGNALS:

		/* internal_signal() is Qt::QueuedConnection to signal() */
		void internal_signal();

		/* finally signal() is emitted in the context of the Qt main thread */
		void signal();
};


/*
 * Genode signal dispatcher thread
 */
class Genode_signal_dispatcher : public Genode::Thread
{
	private:

		Genode::Signal_receiver _sig_rec;

		void entry()
		{
			/* dispatch signals */
			while (true) {
				Genode::Signal sig = _sig_rec.wait_for_signal();
				Genode::Signal_dispatcher_base *dispatcher {
					dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context()) };

				if (dispatcher)
					dispatcher->dispatch(sig.num());
			}
		}

	public:

		Genode_signal_dispatcher(Genode::Env &env)
		:
			Genode::Thread(env, "signal_dispatcher", 0x4000)
		{
			start();
		}

		Genode::Signal_receiver &signal_receiver() { return _sig_rec; }
};


/*
 * Qt initialization
 */

extern void initialize_qt_core(Genode::Env &);
extern void initialize_qt_gui(Genode::Env &);

static inline QApplication & qt5_initialization(Libc::Env &env)
{
	initialize_qt_core(env);
	initialize_qt_gui(env);

	char const *argv[] = { "qt5_app", 0 };
	int argc = sizeof(argv)/sizeof(*argv);

	static QApplication app(argc, (char**)argv);

	QFile file(":style.qss");
	if (!file.open(QFile::ReadOnly)) {
		qWarning() << "Warning:" << file.errorString()
			<< "opening file" << file.fileName();
	} else {
		qApp->setStyleSheet(QLatin1String(file.readAll()));
	}

	app.connect(&app, SIGNAL(lastWindowClosed()), SLOT(quit()));

	return app;
}


/*
 * Widget utilities
 */

struct Spacer : QFrame
{
	Q_OBJECT public:

	Spacer(QString const &style_id = "") { setObjectName(style_id); }

	~Spacer() { }
};

#endif /* _TEST__TILED_WM__UTIL_H_ */
