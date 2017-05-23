/*
 * \brief  A Qt Widget that can load a plugin application and show its Nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef QPLUGINWIDGET_H
#define QPLUGINWIDGET_H

/* Genode includes */
#include <libc/component.h>
#include <loader_session/connection.h>

/* Qt includes */
#include <QtGui>
#include <QtNetwork>

#include <qnitpickerviewwidget/qnitpickerviewwidget.h>

enum Plugin_loading_state
{
	LOADING,
	LOADED,
	NETWORK_ERROR,
	INFLATE_ERROR,
	CAP_QUOTA_EXCEEDED_ERROR,
	RAM_QUOTA_EXCEEDED_ERROR,
	ROM_CONNECTION_FAILED_EXCEPTION,
	TIMEOUT_EXCEPTION
};

class QPluginWidget;

/* separate class, because meta object features are not supported in nested classes */
class PluginStarter : public QThread
{
	Q_OBJECT

	private:

		Libc::Env                  *_env;
		QUrl                        _plugin_url;
		QByteArray                  _args;
		int                         _max_width;
		int                         _max_height;
		Nitpicker::View_capability  _parent_view;

		Loader::Connection         *_pc;
		enum Plugin_loading_state   _plugin_loading_state;
		QString                     _plugin_loading_error_string;

		QNetworkAccessManager      *_qnam;
		QNetworkReply              *_reply;

		void _start_plugin(QString &file_name, QByteArray const &file_buf);

	protected slots:
		void networkReplyFinished();

	public:
		PluginStarter(Libc::Env *env,
		              QUrl plugin_url, QString &args,
		              int max_width, int max_height,
		              Nitpicker::View_capability parent_view);

		void run();
		enum Plugin_loading_state plugin_loading_state() { return _plugin_loading_state; }
		QString &plugin_loading_error_string() { return _plugin_loading_error_string; }

		/**
		 * Requst size of the nitpicker view of the loaded subsystem
		 */
		Loader::Area view_size();

		/**
		 * Set geometry of the nitpicker view of the loaded subsystem
		 */
		void view_geometry(Loader::Rect rect, Loader::Point offset);

	signals:
		void finished();
};


class QPluginWidget : public QEmbeddedViewWidget
{
	Q_OBJECT

	private:

		static Libc::Env          *_env;
		static QPluginWidget      *_last;

		enum Plugin_loading_state  _plugin_loading_state;
		QString                    _plugin_loading_error_string;

		PluginStarter             *_plugin_starter;
		bool                       _plugin_starter_started;

		QUrl                       _plugin_url;
		QString                    _plugin_args;

		int                        _max_width;
		int                        _max_height;

	public:

		enum { PRESERVED_CAPS      = 150 };
		enum { PRESERVED_RAM_QUOTA = 5*1024*1024 };

		void cleanup();

	protected:
		virtual void paintEvent(QPaintEvent *);
		virtual void showEvent(QShowEvent *);
		virtual void hideEvent(QHideEvent *);

	protected slots:
		void pluginStartFinished();

	public:
		QPluginWidget(QWidget *parent, QUrl plugin_url, QString &args,
		              int max_width = -1, int max_height = -1);
		~QPluginWidget();

		static void env(Libc::Env &env) { _env = &env; }
};

#endif // QPLUGINWIDGET_H
