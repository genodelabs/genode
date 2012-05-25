/*
 * \brief   Simple Qt interface for 'avplay' media player
 * \author  Christian Prochaska
 * \date    2012-03-21
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Qt includes */
#include <QApplication>

/* qt_avplay includes */
#include "main_window.h"


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


int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	load_stylesheet();

	QMember<Main_window> main_window;

	main_window->show();

	return app.exec();
}
