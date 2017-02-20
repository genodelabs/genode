/*
 * \brief  A Qt Widget that shows a nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef QNITPICKERVIEWWIDGET_H
#define QNITPICKERVIEWWIDGET_H

#include <QtWidgets>
#if 0
#include <qwindowsystem_qws.h>
#endif

#include <nitpicker_session/client.h>


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


class QNitpickerViewWidget : public QEmbeddedViewWidget
{
	Q_OBJECT

protected:

	Nitpicker::Session_client      *nitpicker;
	Nitpicker::Session::View_handle view_handle;

	virtual void showEvent(QShowEvent *event);
	virtual void hideEvent(QHideEvent *event);
	virtual void paintEvent(QPaintEvent *event);
	virtual void focusInEvent(QFocusEvent * event);

public:

	QNitpickerViewWidget(QWidget *parent =0);
	~QNitpickerViewWidget();
	void setNitpickerView(Nitpicker::Session_client *nitpicker,
	                      Nitpicker::Session::View_handle view_handle,
	                      int buf_x, int buf_y, int w, int h);
};

#endif // QNITPICKERVIEWWIDGET_H
