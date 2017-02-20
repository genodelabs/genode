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


extern int genode_argc;
extern char **genode_argv;

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {
		QApplication app(genode_argc, genode_argv);

		load_stylesheet();

		QMember<Main_window> main_window(env);

		main_window->show();

		app.exec();
	});
}
