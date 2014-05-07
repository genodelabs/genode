/*
 * \brief   Qt Launchpad main program
 * \author  Christian Prochaska
 * \date    2008-04-05
 */

/* local includes */
#include "qt_launchpad.h"

/* Qt includes */
#include <QtGui>
#include <QApplication>

/* Genode includes */
#include <base/env.h>
#include <rom_session/connection.h>

int main(int argc, char *argv[])
{
	/* look for dynamic linker */
	try {
		static Genode::Rom_connection rom("ld.lib.so");
		Genode::Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	int result;

	QApplication *a = new QApplication(argc, argv);

	Qt_launchpad *launchpad = new Qt_launchpad(Genode::env()->ram_session()->quota());

	launchpad->add_launcher("calculatorform", 18*1024*1024);
	launchpad->add_launcher("tetrix",         18*1024*1024);

	launchpad->move(300,100);
	launchpad->show();

	a->connect(a, SIGNAL(lastWindowClosed()), a, SLOT(quit()));

	result = a->exec();

	delete launchpad;
	delete a;

	return result;
}
