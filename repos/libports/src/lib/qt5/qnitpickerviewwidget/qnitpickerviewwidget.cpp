/*
 * \brief  A Qt Widget that shows a nitpicker view
 * \author Christian Prochaska
 * \date   2010-08-26
 */

#include <base/ipc.h>

#include <string.h>

#include <QtGui>
#if 0
#include <private/qwindowsurface_qws_p.h>
#include <private/qwindowsurface_nitpicker_qws_p.h>
#endif

#include <qnitpickerplatformwindow.h>

#include <qnitpickerviewwidget/qnitpickerviewwidget.h>


QNitpickerViewWidget::QNitpickerViewWidget(QWidget *parent)
    : QWidget(parent), nitpicker(0), orig_w(0), orig_h(0), orig_buf_x(0), orig_buf_y(0)
{
}


void QNitpickerViewWidget::setNitpickerView(Nitpicker::Session_client *new_nitpicker,
                                            Nitpicker::Session::View_handle new_view_handle,
                                            int buf_x, int buf_y, int w, int h)
{
	orig_buf_x = buf_x;
	orig_buf_y = buf_y;
	orig_w = w;
	orig_h = h;
//	PDBG("orig_w = %d, orig_h = %d", orig_w, orig_h);

	nitpicker = new_nitpicker;
	view_handle = new_view_handle;
	setFixedSize(orig_w, orig_h);
}


QNitpickerViewWidget::~QNitpickerViewWidget()
{
}


void QNitpickerViewWidget::showEvent(QShowEvent *event)
{
//	qDebug() << "showEvent()";
#if 0
	connect(qwsServer, SIGNAL(windowEvent(QWSWindow*, QWSServer::WindowEvent)),
			this, SLOT(windowEvent(QWSWindow*, QWSServer::WindowEvent)));
#endif
	QWidget::showEvent(event);
}

void QNitpickerViewWidget::hideEvent(QHideEvent *event)
{
//	qDebug() << "hideEvent()";
#if 0
	disconnect(this, SLOT(windowEvent(QWSWindow*, QWSServer::WindowEvent)));
#endif
	QWidget::hideEvent(event);

	if (nitpicker) {

		typedef Nitpicker::Session::Command Command;

		Nitpicker::Rect geometry(Nitpicker::Point(mapToGlobal(pos()).x(),
		                                          mapToGlobal(pos()).y()),
		                         Nitpicker::Area(0, 0));
		nitpicker->enqueue<Command::Geometry>(view_handle, geometry);

		Nitpicker::Point offset(orig_buf_x, orig_buf_y);
		nitpicker->enqueue<Command::Offset>(view_handle, offset);

		nitpicker->execute();
	}
}

void QNitpickerViewWidget::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	if (!nitpicker)
		return;

	/* mark all sliders as unchecked */
	QHashIterator<QScrollBar*, bool> i(_scrollbars);
	while(i.hasNext()) {
		i.next();
		_scrollbars.insert(i.key(), false);
	}

#if 0
//	qDebug() << "paintEvent()";
	qDebug() << "geometry: " << geometry(); /* widget relative to parent widget */
	qDebug() << "rect: " << rect(); /* (0, 0, width(), height()) without frames */
	qDebug() << "event->rect(): " << event->rect();
	qDebug() << "event->region(): " << event->region();
	qDebug() << "global(0, 0): " << mapToGlobal(QPoint(0, 0));
	qDebug() << "x(): " << x() << "y(): " << y();
	qDebug() << "global(x, y): " << mapToGlobal(QPoint(x(), y()));
	qDebug() << "window =" << window();
//	qDebug() << "window->geometry() =" << window()->geometry();
//	qDebug() << "mapToGlobal(window->geometry().topLeft()) =" <<
                mapToGlobal(window()->geometry().topLeft());
	qDebug() << "window->rect().topLeft() =" << window()->rect().topLeft();
	qDebug() << "mapToGlobal(window->rect().topLeft()) =" <<
	            mapToGlobal(window()->rect().topLeft());
	qDebug() << "mapToGlobal(window->childrenRect().topLeft()) =" <<
	            mapToGlobal(window()->childrenRect().topLeft());
	qDebug() << "mapToGlobal(window->contentsRect().topLeft()) =" <<
	            mapToGlobal(window()->contentsRect().topLeft());
	qDebug() << "mapToGlobal(mask().boundingRect().bottomRight()) =" <<
	            mapToGlobal(mask().boundingRect().bottomRight());
#endif

//	QPainter painter(this);
//    painter.fillRect(rect(), Qt::black);

	QWidget *parent = parentWidget();

	int w = 0;
	int h = 0;

	int diff_x = 0;
	int diff_y = 0;

	int x0 = mapToGlobal(QPoint(0, 0)).x();
//	qDebug() << "x0 = " << x0;
	int x1 = mapToGlobal(QPoint(orig_w - 1, 0)).x();
//	qDebug() << "x1 = " << x1;
	int y0 = mapToGlobal(QPoint(0, 0)).y();
//	qDebug() << "y0 = " << y0;
	int y1 = mapToGlobal(QPoint(0, orig_h - 1)).y();
//	qDebug() << "y1 = " << y1;

	while(parent) {
#if 0
		qDebug() << "parent =" << parent;
		qDebug() << "parent's rect:         " << parent->rect();
		qDebug() << "parent's children rect:" << parent->childrenRect();
		qDebug() << "parent's contents rect:" << parent->contentsRect();

		/* start of view = most right window start position */
//		qDebug() << "parent->geometry() =" << parent->geometry();
//		qDebug() << "parent->frameGeometry() =" << parent->frameGeometry();
//		qDebug() << "mapToGlobal(parent->frameGeometry().topLeft()) =" <<
                    parent->mapToGlobal(parent->frameGeometry().topLeft());
		qDebug() << "mapToGlobal(parent->geometry().bottomRight()) =" <<
		            parent->mapToGlobal(parent->geometry().bottomRight());
		qDebug() << "mapToGlobal(parent->rect().topLeft()) =" <<
		            parent->mapToGlobal(parent->rect().topLeft());
		qDebug() << "mapToGlobal(parent->rect().bottomRight()) =" <<
		            parent->mapToGlobal(parent->rect().bottomRight());
		qDebug() << "mapToGlobal(parent->childrenRect().topLeft()) =" <<
		            parent->mapToGlobal(parent->childrenRect().topLeft());
		qDebug() << "mapToGlobal(parent->childrenRect().bottomRight()) =" <<
		            parent->mapToGlobal(parent->childrenRect().bottomRight());
		qDebug() << "mapToGlobal(parent->contentsRect().topLeft()) =" <<
		            parent->mapToGlobal(parent->contentsRect().topLeft());
		qDebug() << "mapToGlobal(parent->contentsRect().bottomRight()) =" <<
		            parent->mapToGlobal(parent->contentsRect().bottomRight());
		qDebug() << "parentWidget()->childAt(" << geometry().topRight() << ") = " <<
		            parentWidget()->childAt(geometry().topRight());
		qDebug() << "visibleRegion() = " << visibleRegion();
		qDebug() << "geometry().contains(" << geometry().topRight() << ") = " <<
		            geometry().contains(geometry().topRight());
	    qDebug() << "mask() = " << mask();
#endif

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

//			w = qMin(w, parent->contentsRect().width() + parent->childrenRect().x());
/*			qDebug() << "mapToGlobal(viewport->rect().topLeft()) =" <<
                        scrollarea->viewport()->mapToGlobal(scrollarea->viewport()->rect().topLeft()); */
/*			qDebug() << "mapToGlobal(viewport->rect().bottomRight()) =" <<
                        scrollarea->viewport()->mapToGlobal(scrollarea->viewport()->rect().bottomRight()); */
		}

		x0 = qMax(x0, parent->mapToGlobal(parent->contentsRect().topLeft()).x());
//		qDebug() << "x0 = " << x0;
		x1 = qMin(x1, parent->mapToGlobal(parent->contentsRect().bottomRight()).x());
//		qDebug() << "x1 = " << x1;
		y0 = qMax(y0, parent->mapToGlobal(parent->contentsRect().topLeft()).y());
//		qDebug() << "y0 = " << y0;
		y1 = qMin(y1, parent->mapToGlobal(parent->contentsRect().bottomRight()).y());
//		qDebug() << "y1 = " << y1;

		//w = qMin(w, parent->contentsRect().width() /*+ parent->childrenRect().x()*/);
		w = x1 - x0 + 1;
//		qDebug() << "w = " << w;
		h = y1 - y0 + 1;
//		qDebug() << "h = " << h;

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

	typedef Nitpicker::Session::Command Command;

	/* argument to mapToGlobal() is relative to the Widget's origin
	 * the plugin view starts at (0, 0)
	 */
	if (mask().isEmpty()) {
//		PDBG("x0 = %d, y0 = %d, w = %d, h = %d, buf_x = %d, buf_y = %d",
//             x0, y0, w, h, orig_buf_x + diff_x, orig_buf_y + diff_y);

		Nitpicker::Rect geometry(Nitpicker::Point(x0, y0),
		                         Nitpicker::Area(w, h));
		nitpicker->enqueue<Command::Geometry>(view_handle, geometry);
		Nitpicker::Point offset(orig_buf_x + diff_x, orig_buf_y + diff_y);
		nitpicker->enqueue<Command::Offset>(view_handle, offset);
	} else {
/*		PDBG("x = %d, y = %d, w = %d, h = %d, buf_x = %d, buf_y = %d",
             mapToGlobal(mask().boundingRect().topLeft()).x(),
             mapToGlobal(mask().boundingRect().topLeft()).y(),
             mask().boundingRect().width(),
             mask().boundingRect().height(),
             orig_buf_x + diff_x, orig_buf_y + diff_y); */

		Nitpicker::Rect const
			geometry(Nitpicker::Point(mapToGlobal(mask().boundingRect().topLeft()).x(),
			                          mapToGlobal(mask().boundingRect().topLeft()).y()),
			         Nitpicker::Area(mask().boundingRect().width(),
			                         mask().boundingRect().height()));
		nitpicker->enqueue<Command::Geometry>(view_handle, geometry);

		Nitpicker::Point offset(orig_buf_x + diff_x, orig_buf_y + diff_y);
		nitpicker->enqueue<Command::Offset>(view_handle, offset);
	}

	/* bring the plugin view to the front of the Qt window */
	QNitpickerPlatformWindow *platform_window =
		dynamic_cast<QNitpickerPlatformWindow*>(window()->windowHandle()->handle());

	Nitpicker::Session::View_handle neighbor_handle =
		nitpicker->view_handle(platform_window->view_cap());

	nitpicker->enqueue<Command::To_front>(view_handle, neighbor_handle);
	nitpicker->execute();

	nitpicker->release_view_handle(neighbor_handle);
}
#if 0
void QNitpickerViewWidget::windowEvent(QWSWindow *window,
                                       QWSServer::WindowEvent eventType)
{
	if (this->window()->windowSurface() && (window->winId() == static_cast<QWSWindowSurface*>(this->window()->windowSurface())->winId())) {

		switch (eventType) {

			/* the window has changed its geometry */
			case QWSServer::Geometry:
			{
				if (isVisible()) {
					QPaintEvent e(rect());
					paintEvent(&e);
				}
				break;
			}

			case QWSServer::Raise:
			{
				if (vc) {
					try {
						vc->stack(Nitpicker::View_capability(), true, true);
					} catch (Genode::Ipc_error) {
						delete vc;
						vc = 0;
					}
				}
				break;
			}

			default:
				break;
		}
	}
}
#endif
void QNitpickerViewWidget::valueChanged()
{
//	qDebug() << "valueChanged()";
	if (isVisible()) {
		QPaintEvent e(rect());
		paintEvent(&e);
	}
}

void QNitpickerViewWidget::destroyed(QObject *obj)
{
	_scrollbars.remove(qobject_cast<QScrollBar*>(obj));
}
