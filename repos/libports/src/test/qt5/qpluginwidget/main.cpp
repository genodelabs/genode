/*
 * \brief   QPluginWidget test
 * \author  Christian Prochaska
 * \date    2012-04-23
 */

/*
 * Copyright (C) 2012-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Qt includes */
#include <QtGui>
#include <QApplication>

/* Qoost includes */
#include <qoost/compound_widget.h>

/* qpluginwidget includes */
#include <qpluginwidget/qpluginwidget.h>

/* qt5_component includes */
#include <qt5_component/qpa_init.h>

void Libc::Component::construct(Libc::Env &env)
{
	Libc::with_libc([&] {

		qpa_init(env);

		int argc = 1;
		char const *argv[] = { "test-qpluginwidget", 0 };

		QApplication app(argc, (char**)argv);

		Compound_widget<QWidget, QHBoxLayout> w;

		QPluginLoader plugin_loader("/qt/plugins/qpluginwidget/libqpluginwidget.lib.so");

		QObject *plugin = plugin_loader.instance();

		if (!plugin)
			qFatal("Error: Could not load QPluginWidget Qt plugin");

		QPluginWidgetInterface *plugin_widget_interface = qobject_cast<QPluginWidgetInterface*>(plugin);

		plugin_widget_interface->env(env);

		QString plugin_args("ram_quota=6M, caps=500");

		QPluginWidget *plugin_widget = static_cast<QPluginWidget*>(
			plugin_widget_interface->createWidget(&w,
			                                      QUrl("rom:///test-plugin.tar"),
			                                      plugin_args, 100, 100)
		);

		w.layout()->addWidget(plugin_widget);

		w.resize(150, 150);

		w.show();

		return app.exec();
	});
}


/* dummy will not be called */
int main(int argc, char *argv[])
{
	qFatal("Error: '", __func__, "' should not be called.");
	return -1;
}
