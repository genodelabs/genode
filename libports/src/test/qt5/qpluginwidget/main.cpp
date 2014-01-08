/*
 * \brief   QPluginWidget test
 * \author  Christian Prochaska
 * \date    2012-04-23
 */

/* Qt includes */
#include <QtGui>
#include <QApplication>
#include <qpluginwidget/qpluginwidget.h>

/* Qoost includes */
#include <qoost/compound_widget.h>

int main(int argc, char *argv[])
{
	static QApplication app(argc, argv);

	static Compound_widget<QWidget, QHBoxLayout> w;

	static QString plugin_args("ram_quota=3M");
	static QPluginWidget plugin_widget(&w, QUrl("rom:///test-plugin.tar"), plugin_args, 100, 100);

	w.layout()->addWidget(&plugin_widget);
	w.resize(150, 150);

	w.show();

	return app.exec();
}
