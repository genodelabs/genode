/*
 * \brief  A Qt Widget that can load a plugin application and show its Nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef QPLUGINWIDGET_H
#define QPLUGINWIDGET_H

#include <QtGui>
#include <QtNetwork>

#include <loader_session/connection.h>

#include <qnitpickerviewwidget/qnitpickerviewwidget.h>


enum Plugin_loading_state
{
	LOADING,
	LOADED,
	NETWORK_ERROR,
	INFLATE_ERROR,
	QUOTA_EXCEEDED_ERROR,
	ROM_CONNECTION_FAILED_EXCEPTION,
	TIMEOUT_EXCEPTION
};

class QPluginWidget;

/* separate class, because meta object features are not supported in nested classes */
class PluginStarter : public QThread
{
	Q_OBJECT

	private:
		QUrl _plugin_url;
		QByteArray _args;
		int _max_width;
		int _max_height;

		Loader::Connection *_pc;
		enum Plugin_loading_state _plugin_loading_state;
		QString _plugin_loading_error_string;

		QNetworkAccessManager *_qnam;
		QNetworkReply *_reply;

		void _start_plugin(QString &file_name, QByteArray const &file_buf);

	protected slots:
		void networkReplyFinished();

	public:
		PluginStarter(QUrl plugin_url, QString &args,
					  int max_width, int max_height);

		void run();
		enum Plugin_loading_state plugin_loading_state() { return _plugin_loading_state; }
		QString &plugin_loading_error_string() { return _plugin_loading_error_string; }
		Nitpicker::View_capability plugin_view(int *w, int *h, int *buf_x, int *buf_y);

	signals:
		void finished();
};


class QPluginWidget : public QNitpickerViewWidget
{
	Q_OBJECT

	private:

		enum Plugin_loading_state _plugin_loading_state;
		QString _plugin_loading_error_string;

		PluginStarter *_plugin_starter;

		int _max_width;
		int _max_height;

		static QPluginWidget *_last;

	public:
		enum { RAM_QUOTA = 5*1024*1024 };

		void cleanup();

	protected:
		virtual void paintEvent(QPaintEvent *event);

	protected slots:
		void pluginStartFinished();

	public:
		QPluginWidget(QWidget *parent, QUrl plugin_url, QString &args, int max_width = -1, int max_height = -1);
		~QPluginWidget();
};

#endif // QPLUGINWIDGET_H
