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

extern void initialize_qt_core(Genode::Env &);
extern void initialize_qt_gui(Genode::Env &);

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		initialize_qt_core(env);
		initialize_qt_gui(env);
		QPluginWidget::env(env);

		int argc = 1;
		char const *argv[] = { "test-qpluginwidget", 0 };

		QApplication app(argc, (char**)argv);

		Compound_widget<QWidget, QHBoxLayout> w;

		QString plugin_args("ram_quota=4M, caps=500");
		QPluginWidget plugin_widget(&w, QUrl("rom:///test-plugin.tar"),
		                            plugin_args, 100, 100);

		w.layout()->addWidget(&plugin_widget);
		w.resize(150, 150);

		w.show();

		return app.exec();
	});
}
