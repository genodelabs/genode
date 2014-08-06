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

	static QApplication a(argc, argv);

	static Qt_launchpad launchpad(Genode::env()->ram_session()->quota());

	launchpad.add_launcher("calculatorform",
	                       30*1024*1024,
	                       Genode::Dataspace_capability());

	launchpad.add_launcher("tetrix",
	                       40*1024*1024,
	                       Genode::Dataspace_capability());

	launchpad.move(300,100);
	launchpad.show();

	a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));

	result = a.exec();

	return result;
}
