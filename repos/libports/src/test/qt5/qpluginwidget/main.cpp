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

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		QPluginWidget::set_env(&env);

		int argc = 1;
		char *argv[] = { "test-qpluginwidget", 0 };

		QApplication app(argc, argv);

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
