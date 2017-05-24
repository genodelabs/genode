/*
 * \brief   Simple Qt interface for 'avplay' media player
 * \author  Christian Prochaska
 * \date    2012-03-21
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Qt includes */
#include <QApplication>
#include <qnitpickerintegrationplugin.h>

/* qt_avplay includes */
#include "main_window.h"

/* Genode includes */
#include <libc/component.h>


static inline void load_stylesheet()
{
        QFile file(":style.qss");
        if (!file.open(QFile::ReadOnly)) {
                qWarning() << "Warning:" << file.errorString()
                           << "opening file" << file.fileName();
                return;
        }

        qApp->setStyleSheet(QLatin1String(file.readAll()));
}

extern void initialize_qt_core(Genode::Env &);
extern void initialize_qt_gui(Genode::Env &);

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		initialize_qt_core(env);
		initialize_qt_gui(env);

		int argc = 1;
		char const *argv[] = { "qt_avplay", 0 };

		QApplication app(argc, (char**)argv);

		load_stylesheet();

		QMember<Main_window> main_window(env);

		main_window->show();

		app.exec();
	});
}
