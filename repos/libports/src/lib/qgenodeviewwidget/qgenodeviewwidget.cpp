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

#include <base/ipc.h>

#include <string.h>

#include <QtGui>

#include <qpa_genode/qgenodeplatformwindow.h>
#include <qgenodeviewwidget/qgenodeviewwidget.h>


/*************************
 ** QEmbeddedViewWidget **
 *************************/

QEmbeddedViewWidget::QEmbeddedViewWidget(QWidget *) { }


QEmbeddedViewWidget::~QEmbeddedViewWidget() { }


QEmbeddedViewWidget::View_geometry QEmbeddedViewWidget::_calc_view_geometry()
{
	/* mark all sliders as unchecked */
	QHashIterator<QScrollBar*, bool> i(_scrollbars);
	while(i.hasNext()) {
		i.next();
		_scrollbars.insert(i.key(), false);
	}

	QWidget *parent = parentWidget();

	int w = 0;
	int h = 0;

	int diff_x = 0;
	int diff_y = 0;

	int x0 = mapToGlobal(QPoint(0, 0)).x();
	int x1 = mapToGlobal(QPoint(_orig_w - 1, 0)).x();
	int y0 = mapToGlobal(QPoint(0, 0)).y();
	int y1 = mapToGlobal(QPoint(0, _orig_h - 1)).y();

	while(parent) {

		if (parent->inherits("QAbstractScrollArea")) {
			QAbstractScrollArea *scrollarea = qobject_cast<QAbstractScrollArea*>(parent);
			QScrollBar *scrollbar;

			scrollbar = scrollarea->horizontalScrollBar();
			if (!_scrollbars.contains(scrollbar)) {
				connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(valueChanged()));
				connect(scrollbar, SIGNAL(destroyed(QObject*)), this, SLOT(destroyed(QObject*)));
			}
			/* update/insert value */
			_scrollbars.insert(scrollbar, true);

			scrollbar = scrollarea->verticalScrollBar();
			if (!_scrollbars.contains(scrollbar)) {
				connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(valueChanged()));
				connect(scrollbar, SIGNAL(destroyed(QObject*)), this, SLOT(destroyed(QObject*)));
			}
			/* update/insert value */
			_scrollbars.insert(scrollbar, true);
		}

		x0 = qMax(x0, parent->mapToGlobal(parent->contentsRect().topLeft()).x());
		x1 = qMin(x1, parent->mapToGlobal(parent->contentsRect().bottomRight()).x());
		y0 = qMax(y0, parent->mapToGlobal(parent->contentsRect().topLeft()).y());
		y1 = qMin(y1, parent->mapToGlobal(parent->contentsRect().bottomRight()).y());

		w = x1 - x0 + 1;
		h = y1 - y0 + 1;

		if (parent->childrenRect().x() < 0) {
			diff_x += parent->childrenRect().x();
		}

		if (parent->childrenRect().y() < 0) {
			diff_y += parent->childrenRect().y();
		}

		parent = parent->parentWidget();
	}

	/* disconnect and remove any scrollbar that is not in the hierarchy anymore */
	i.toBack();
	while(i.hasNext()) {
		i.next();
		if (_scrollbars.value(i.key()) == false) {
			disconnect(i.key(), SIGNAL(valueChanged(int)), this, SLOT(valueChanged()));
			disconnect(i.key(), SIGNAL(destroyed(QObject*)), this, SLOT(destroyed(QObject*)));
			_scrollbars.remove(i.key());
		}
	}

	return QEmbeddedViewWidget::View_geometry { x0, y0, w, h,
	                                            _orig_buf_x + diff_x,
	                                            _orig_buf_y + diff_y };
}


void QEmbeddedViewWidget::valueChanged()
{
	if (isVisible()) {
		QPaintEvent e(rect());
		paintEvent(&e);
	}
}


void QEmbeddedViewWidget::destroyed(QObject *obj)
{
	_scrollbars.remove(qobject_cast<QScrollBar*>(obj));
}


/**************************
 ** QGenodeViewWidget **
 **************************/

QGenodeViewWidget::QGenodeViewWidget(QWidget *parent)
:
	QEmbeddedViewWidget(parent), gui(0)
{ }


void QGenodeViewWidget::setGenodeView(Gui::Connection *new_gui,
                                      Gui::View_id new_view_id,
                                      int buf_x, int buf_y, int w, int h)
{
	QEmbeddedViewWidget::_orig_geometry(w, h, buf_x, buf_y);

	gui = new_gui;
	view_id = new_view_id;
	setFixedSize(w, h);
}


QGenodeViewWidget::~QGenodeViewWidget() { }


void QGenodeViewWidget::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);
}


void QGenodeViewWidget::hideEvent(QHideEvent *event)
{
	QWidget::hideEvent(event);

	if (gui) {

		QEmbeddedViewWidget::View_geometry const view_geometry =
			QEmbeddedViewWidget::_calc_view_geometry();

		using Command = Gui::Session::Command;

		Gui::Rect const geometry(Gui::Point(mapToGlobal(pos()).x(),
		                                    mapToGlobal(pos()).y()),
		                         Gui::Area(0, 0));
		gui->enqueue<Command::Geometry>(view_id, geometry);

		Gui::Point const offset(view_geometry.buf_x, view_geometry.buf_y);
		gui->enqueue<Command::Offset>(view_id, offset);

		gui->execute();
	}
}


void QGenodeViewWidget::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	if (!gui)
		return;

	QEmbeddedViewWidget::View_geometry const view_geometry =
		QEmbeddedViewWidget::_calc_view_geometry();

	using Command = Gui::Session::Command;

	/* argument to mapToGlobal() is relative to the Widget's origin
	 * the plugin view starts at (0, 0)
	 */
	if (mask().isEmpty()) {

		Gui::Rect const geometry(Gui::Point(view_geometry.x, view_geometry.y),
		                         Gui::Area(view_geometry.w, view_geometry.h));
		gui->enqueue<Command::Geometry>(view_id, geometry);
		Gui::Point const offset(view_geometry.buf_x, view_geometry.buf_y);
		gui->enqueue<Command::Offset>(view_id, offset);
	} else {

		Gui::Rect const
			geometry(Gui::Point(mapToGlobal(mask().boundingRect().topLeft()).x(),
			                    mapToGlobal(mask().boundingRect().topLeft()).y()),
			         Gui::Area(mask().boundingRect().width(),
			                   mask().boundingRect().height()));
		gui->enqueue<Command::Geometry>(view_id, geometry);

		Gui::Point const offset(view_geometry.buf_x, view_geometry.buf_y);
		gui->enqueue<Command::Offset>(view_id, offset);
	}

	/* bring the plugin view to the front of the Qt window */
	QGenodePlatformWindow *platform_window =
		dynamic_cast<QGenodePlatformWindow*>(window()->windowHandle()->handle());

	Gui::View_id const neighbor_id =
		gui->alloc_view_id(platform_window->view_cap());

	gui->enqueue<Command::Front_of>(view_id, neighbor_id);
	gui->execute();

	gui->release_view_id(neighbor_id);
}


void QGenodeViewWidget::focusInEvent(QFocusEvent *)
{
	QGenodePlatformWindow *platform_window =
		dynamic_cast<QGenodePlatformWindow*>(window()->windowHandle()->handle());

	platform_window->gui_session().focus(gui->cap());
}
