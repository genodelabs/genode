/*
 * \brief  A Qt Widget that shows a Genode GUI view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef QGENODEVIEWWIDGET_H
#define QGENODEVIEWWIDGET_H

#include <QtWidgets>

#include <gui_session/connection.h>


class QEmbeddedViewWidget : public QWidget
{
	Q_OBJECT

private:

	QHash<QScrollBar*, bool> _scrollbars;

	int _orig_w = 0;
	int _orig_h = 0;
	int _orig_buf_x = 0;
	int _orig_buf_y = 0;

private slots:

	void valueChanged();
	void destroyed(QObject *obj = 0);

protected:

	struct View_geometry
	{
		int x, y, w, h, buf_x, buf_y;
	};

	QEmbeddedViewWidget(QWidget *parent = 0);

	virtual ~QEmbeddedViewWidget();

	void _orig_geometry(int w, int h, int buf_x, int buf_y)
	{
		_orig_w = w;
		_orig_h = h;
		_orig_buf_x = buf_x;
		_orig_buf_y = buf_y;
	}

	View_geometry _calc_view_geometry();
};


class QGenodeViewWidget : public QEmbeddedViewWidget
{
	Q_OBJECT

protected:

	Gui::Connection *gui;
	Gui::View_id     view_id;

	virtual void showEvent(QShowEvent *event);
	virtual void hideEvent(QHideEvent *event);
	virtual void paintEvent(QPaintEvent *event);
	virtual void focusInEvent(QFocusEvent * event);

public:

	QGenodeViewWidget(QWidget *parent =0);
	~QGenodeViewWidget();
	void setGenodeView(Gui::Connection *gui, Gui::View_id view_id,
	                   int buf_x, int buf_y, int w, int h);
};

class QGenodeViewWidgetInterface
{
	public:
		virtual QWidget *createWidget(QWidget *parent = 0) = 0;
};


Q_DECLARE_INTERFACE(QGenodeViewWidgetInterface, "org.genode.QGenodeViewWidgetInterface")


class QGenodeViewWidgetPlugin : public QObject, public QGenodeViewWidgetInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "org.genode.QGenodeViewWidgetInterface" FILE "qgenodeviewwidget.json")
	Q_INTERFACES(QGenodeViewWidgetInterface)

	public:

		explicit QGenodeViewWidgetPlugin(QObject *parent = 0) : QObject(parent) { }

		QWidget *createWidget(QWidget *parent = 0)
		{
			return new QGenodeViewWidget(parent);
		}
	
};

#endif // QGENODEVIEWWIDGET_H
