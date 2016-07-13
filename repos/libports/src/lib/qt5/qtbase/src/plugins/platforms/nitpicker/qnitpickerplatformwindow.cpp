/*
 * \brief  QNitpickerPlatformWindow
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */


/* Genode includes */
#include <base/log.h>
#include <nitpicker_session/client.h>

/* Qt includes */
#include <qpa/qplatformscreen.h>
#include <QGuiApplication>
#include <QDebug>

#include "qnitpickerplatformwindow.h"

QT_BEGIN_NAMESPACE

static const bool qnpw_verbose = false/*true*/;

QTouchDevice * QNitpickerPlatformWindow::_init_touch_device()
{
	QVector<QWindowSystemInterface::TouchPoint>::iterator i = _touch_points.begin();
	for (unsigned n = 0; i != _touch_points.end(); ++i, ++n) {
		i->id    = n;
		i->state = Qt::TouchPointReleased;
	}

	QTouchDevice *dev = new QTouchDevice;

	dev->setName("Genode multi-touch device");
	dev->setType(QTouchDevice::TouchScreen);
	dev->setCapabilities(QTouchDevice::Position);

	QWindowSystemInterface::registerTouchDevice(dev);

	return dev;
}

void QNitpickerPlatformWindow::_process_touch_events(QList<Input::Event> const &events)
{
	if (events.empty()) return;

	QList<QWindowSystemInterface::TouchPoint> touch_points;
	for (QList<Input::Event>::const_iterator i = events.begin(); i != events.end(); ++i) {

		/*
		 * Coordinates must be normalized to positions of the platform window.
		 * We lack information about the value ranges (min and max) of touch
		 * coordinates to normalize ourselves.
		 */
		QPointF const pos((qreal)i->ax(), (qreal)i->ay() );

		QWindowSystemInterface::TouchPoint &otp = _touch_points[i->code()];
		QWindowSystemInterface::TouchPoint tp;

		tp.id   = i->code();
		tp.area = QRectF(QPointF(0, 0), QSize(1, 1));

		/* report 1x1 rectangular area centered at screen coordinates */
		tp.area.moveCenter(QPointF(i->ax(), i->ay()));

		if (i->rx() == -1 && i->ry() == -1) {
			tp.state    = Qt::TouchPointReleased;
			tp.pressure = 0;
		} else {
			tp.state    = otp.state == Qt::TouchPointReleased
			            ? Qt::TouchPointPressed : Qt::TouchPointMoved;
			tp.pressure = 1;
		}

		otp = tp;
		touch_points.push_back(tp);
	}

	QWindowSystemInterface::handleTouchEvent(0, _touch_device, touch_points);
}

void QNitpickerPlatformWindow::_process_mouse_event(Input::Event *ev)
{
	QPoint global_position(ev->ax(), ev->ay());
	QPoint local_position(global_position.x() - geometry().x(),
			              global_position.y() - geometry().y());

	switch (ev->type()) {

		case Input::Event::PRESS:

			/* make this window the focused window */
			requestActivateWindow();

			switch (ev->code()) {
				case Input::BTN_LEFT:
					_mouse_button_state |= Qt::LeftButton;
					break;
				case Input::BTN_RIGHT:
					_mouse_button_state |= Qt::RightButton;
					break;
				case Input::BTN_MIDDLE:
					_mouse_button_state |= Qt::MidButton;
					break;
				case Input::BTN_SIDE:
					_mouse_button_state |= Qt::XButton1;
					break;
				case Input::BTN_EXTRA:
					_mouse_button_state |= Qt::XButton2;
					break;
			}
			break;

		case Input::Event::RELEASE:

			switch (ev->code()) {
				case Input::BTN_LEFT:
					_mouse_button_state &= ~Qt::LeftButton;
					break;
				case Input::BTN_RIGHT:
					_mouse_button_state &= ~Qt::RightButton;
					break;
				case Input::BTN_MIDDLE:
					_mouse_button_state &= ~Qt::MidButton;
					break;
				case Input::BTN_SIDE:
					_mouse_button_state &= ~Qt::XButton1;
					break;
				case Input::BTN_EXTRA:
					_mouse_button_state &= ~Qt::XButton2;
					break;
			}
			break;

		case Input::Event::WHEEL:

			QWindowSystemInterface::handleWheelEvent(window(),
					                                 local_position,
					                                 local_position,
					                                 ev->ry() * 120,
					                                 Qt::Vertical);
			return;

		default:
			break;
	}

	QWindowSystemInterface::handleMouseEvent(window(),
			                                 local_position,
			                                 global_position,
			                                 _mouse_button_state);
}


void QNitpickerPlatformWindow::_process_key_event(Input::Event *ev)
{
	const bool pressed = (ev->type() == Input::Event::PRESS);
	const int keycode = ev->code();

	if (pressed) {
		_last_keycode = keycode;
		_key_repeat_timer->start(KEY_REPEAT_DELAY_MS);
	} else
		_key_repeat_timer->stop();

	_keyboard_handler.processKeycode(keycode, pressed, false);
}


void QNitpickerPlatformWindow::_key_repeat()
{
	_key_repeat_timer->start(KEY_REPEAT_RATE_MS);
	_keyboard_handler.processKeycode(_last_keycode, true, true);
}


void QNitpickerPlatformWindow::_handle_input(unsigned int)
{
	QList<Input::Event> touch_events;
	for (int i = 0, num_ev = _input_session.flush(); i < num_ev; i++) {

		Input::Event *ev = &_ev_buf[i];

		bool const is_key_event = ev->type() == Input::Event::PRESS ||
					              ev->type() == Input::Event::RELEASE;

		bool const is_mouse_button_event =
			is_key_event && (ev->code() == Input::BTN_LEFT ||
			                 ev->code() == Input::BTN_MIDDLE ||
			                 ev->code() == Input::BTN_RIGHT);

		if (ev->type() == Input::Event::MOTION ||
			ev->type() == Input::Event::WHEEL ||
			is_mouse_button_event) {

			_process_mouse_event(ev);

		} else if (ev->type() == Input::Event::TOUCH) {

			touch_events.push_back(*ev);

		} else if (is_key_event && (ev->code() < 128)) {

			_process_key_event(ev);

		}
	}

	/* process all gathered touch events */
	_process_touch_events(touch_events);
}


void QNitpickerPlatformWindow::_handle_mode_changed(unsigned int)
{
	Framebuffer::Mode mode(_nitpicker_session.mode());

	if ((mode.width() == 0) && (mode.height() == 0)) {
		/* interpret a size of 0x0 as indication to close the window */
		QWindowSystemInterface::handleCloseEvent(window(), 0);
	}

	if ((mode.width() != _current_mode.width()) ||
	    (mode.height() != _current_mode.height()) ||
	    (mode.format() != _current_mode.format())) {

		QRect geo(geometry());
		geo.setWidth(mode.width());
		geo.setHeight(mode.height());

		QWindowSystemInterface::handleGeometryChange(window(), geo);

		setGeometry(geo);
	}
}


Nitpicker::Session::View_handle QNitpickerPlatformWindow::_create_view()
{
	if (window()->type() == Qt::Desktop)
		return Nitpicker::Session::View_handle();

	if (window()->type() == Qt::Dialog)
		return _nitpicker_session.create_view();

	/*
	 * Popup menus should never get a window decoration, therefore we set a top
	 * level Qt window as 'transient parent'.
	 */
	if (!window()->transientParent() && (window()->type() == Qt::Popup)) {
		QWindow *top_window = QGuiApplication::topLevelWindows().first();
	    window()->setTransientParent(top_window);
	}

	if (window()->transientParent()) {
		QNitpickerPlatformWindow *parent_platform_window =
			static_cast<QNitpickerPlatformWindow*>(window()->transientParent()->handle());

		Nitpicker::Session::View_handle parent_handle =
			_nitpicker_session.view_handle(parent_platform_window->view_cap());

		Nitpicker::Session::View_handle result =
			_nitpicker_session.create_view(parent_handle);

		_nitpicker_session.release_view_handle(parent_handle);

		return result;
	}

	return _nitpicker_session.create_view();
}

void QNitpickerPlatformWindow::_adjust_and_set_geometry(const QRect &rect)
{
	/* limit window size to screen size */
	QRect adjusted_rect(rect.intersected(screen()->geometry()));

	/* Currently, top level windows must start at (0,0) */
	if (!window()->transientParent())
		adjusted_rect.moveTo(0, 0);

	QPlatformWindow::setGeometry(adjusted_rect);

	Framebuffer::Mode mode(adjusted_rect.width(), adjusted_rect.height(),
	                       Framebuffer::Mode::RGB565);
	_nitpicker_session.buffer(mode, false);

	_current_mode = mode;

	_framebuffer_changed = true;
	_geometry_changed = true;

	emit framebuffer_changed();
}

QNitpickerPlatformWindow::QNitpickerPlatformWindow(QWindow *window,
                                                   Genode::Signal_receiver &signal_receiver,
                                                   int screen_width, int screen_height)
: QPlatformWindow(window),
  _framebuffer_session(_nitpicker_session.framebuffer_session()),
  _framebuffer(0),
  _framebuffer_changed(false),
  _geometry_changed(false),
  _signal_receiver(signal_receiver),
  _view_handle(_create_view()),
  _input_session(_nitpicker_session.input_session()),
  _keyboard_handler("", -1, false, false, ""),
  _resize_handle(!window->flags().testFlag(Qt::Popup)),
  _decoration(!window->flags().testFlag(Qt::Popup)),
  _egl_surface(EGL_NO_SURFACE),
  _key_repeat_timer(this),
  _last_keycode(0),
  _input_signal_dispatcher(_signal_receiver, *this,
                           &QNitpickerPlatformWindow::_input),
  _mode_changed_signal_dispatcher(_signal_receiver, *this,
                                  &QNitpickerPlatformWindow::_mode_changed),
  _touch_device(_init_touch_device())
{
	if (qnpw_verbose)
		if (window->transientParent())
			qDebug() << "QNitpickerPlatformWindow(): child window of" << window->transientParent();

	_input_session.sigh(_input_signal_dispatcher);

	_nitpicker_session.mode_sigh(_mode_changed_signal_dispatcher);

	_adjust_and_set_geometry(geometry());

	_ev_buf = static_cast<Input::Event *>
			  (Genode::env()->rm_session()->attach(_input_session.dataspace()));

	if (_view_handle.valid()) {

		/* bring the view to the top */
		typedef Nitpicker::Session::Command Command;
		_nitpicker_session.enqueue<Command::To_front>(_view_handle);
		_nitpicker_session.execute();
	}

	connect(this, SIGNAL(_input(unsigned int)),
	        this, SLOT(_handle_input(unsigned int)),
	        Qt::QueuedConnection);

	connect(this, SIGNAL(_mode_changed(unsigned int)),
	        this, SLOT(_handle_mode_changed(unsigned int)),
	        Qt::QueuedConnection);

	connect(_key_repeat_timer, SIGNAL(timeout()),
	        this, SLOT(_key_repeat()));
}

QWindow *QNitpickerPlatformWindow::window() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::window()";
	return QPlatformWindow::window();
}

QPlatformWindow *QNitpickerPlatformWindow::parent() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::parent()";
	return QPlatformWindow::parent();
}

QPlatformScreen *QNitpickerPlatformWindow::screen() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::screen()";
	return QPlatformWindow::screen();
}

QSurfaceFormat QNitpickerPlatformWindow::format() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::format()";
	return QPlatformWindow::format();
}

void QNitpickerPlatformWindow::setGeometry(const QRect &rect)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setGeometry(" << rect << ")";

	_adjust_and_set_geometry(rect);

	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setGeometry() finished";
}

QRect QNitpickerPlatformWindow::geometry() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::geometry(): returning" << QPlatformWindow::geometry();
	return QPlatformWindow::geometry();
}

QMargins QNitpickerPlatformWindow::frameMargins() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::frameMargins()";
	return QPlatformWindow::frameMargins();
}

void QNitpickerPlatformWindow::setVisible(bool visible)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setVisible(" << visible << ")";

	typedef Nitpicker::Session::Command Command;

	if (visible) {
		QRect g(geometry());

		if (window()->transientParent()) {
			/* translate global position to parent-relative position */
			g.moveTo(window()->transientParent()->mapFromGlobal(g.topLeft()));
		}

		_nitpicker_session.enqueue<Command::Geometry>(_view_handle,
		     Nitpicker::Rect(Nitpicker::Point(g.x(), g.y()),
		                     Nitpicker::Area(g.width(), g.height())));
	} else {

		_nitpicker_session.enqueue<Command::Geometry>(_view_handle,
		     Nitpicker::Rect(Nitpicker::Point(), Nitpicker::Area(0, 0)));
	}

	_nitpicker_session.execute();



	QPlatformWindow::setVisible(visible);

	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setVisible() finished";
}

void QNitpickerPlatformWindow::setWindowFlags(Qt::WindowFlags flags)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowFlags(" << flags << ")";

	QPlatformWindow::setWindowFlags(flags);

	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowFlags() finished";
}

void QNitpickerPlatformWindow::setWindowState(Qt::WindowState state)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowState(" << state << ")";
	QPlatformWindow::setWindowState(state);
}

WId QNitpickerPlatformWindow::winId() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::winId()";
	return WId(this);
}

void QNitpickerPlatformWindow::setParent(const QPlatformWindow *window)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setParent()";
	QPlatformWindow::setParent(window);
}

void QNitpickerPlatformWindow::setWindowTitle(const QString &title)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowTitle(" << title << ")";

	QPlatformWindow::setWindowTitle(title);

	_title = title.toLocal8Bit();

	typedef Nitpicker::Session::Command Command;

	if (_view_handle.valid()) {
		_nitpicker_session.enqueue<Command::Title>(_view_handle, _title.constData());
		_nitpicker_session.execute();
	}

	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowTitle() finished";
}

void QNitpickerPlatformWindow::setWindowFilePath(const QString &title)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowFilePath(" << title << ")";
	QPlatformWindow::setWindowFilePath(title);
}

void QNitpickerPlatformWindow::setWindowIcon(const QIcon &icon)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowIcon()";
	QPlatformWindow::setWindowIcon(icon);
}

void QNitpickerPlatformWindow::raise()
{
	/* bring the view to the top */
	_nitpicker_session.enqueue<Nitpicker::Session::Command::To_front>(_view_handle);
	_nitpicker_session.execute();

	QPlatformWindow::raise();
}

void QNitpickerPlatformWindow::lower()
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::lower()";
	QPlatformWindow::lower();
}

bool QNitpickerPlatformWindow::isExposed() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::isExposed()";
	return QPlatformWindow::isExposed();
}

bool QNitpickerPlatformWindow::isActive() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::isActive()";
	return QPlatformWindow::isActive();
}

bool QNitpickerPlatformWindow::isEmbedded(const QPlatformWindow *parentWindow) const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::isEmbedded()";
	return QPlatformWindow::isEmbedded(parentWindow);
}

QPoint QNitpickerPlatformWindow::mapToGlobal(const QPoint &pos) const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::mapToGlobal(" << pos << ")";
	return QPlatformWindow::mapToGlobal(pos);
}

QPoint QNitpickerPlatformWindow::mapFromGlobal(const QPoint &pos) const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::mapFromGlobal(" << pos << ")";
	return QPlatformWindow::mapFromGlobal(pos);
}

void QNitpickerPlatformWindow::propagateSizeHints()
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::propagateSizeHints()";
	QPlatformWindow::propagateSizeHints();
}

void QNitpickerPlatformWindow::setOpacity(qreal level)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setOpacity(" << level << ")";
	QPlatformWindow::setOpacity(level);
}

void QNitpickerPlatformWindow::setMask(const QRegion &region)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setMask(" << region << ")";
	QPlatformWindow::setMask(region);
}

void QNitpickerPlatformWindow::requestActivateWindow()
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::requestActivateWindow()";
	QPlatformWindow::requestActivateWindow();
}

void QNitpickerPlatformWindow::handleContentOrientationChange(Qt::ScreenOrientation orientation)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::handleContentOrientationChange()";
	QPlatformWindow::handleContentOrientationChange(orientation);
}

qreal QNitpickerPlatformWindow::devicePixelRatio() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::devicePixelRatio()";
	return QPlatformWindow::devicePixelRatio();
}

bool QNitpickerPlatformWindow::setKeyboardGrabEnabled(bool grab)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setKeyboardGrabEnabled()";
	return QPlatformWindow::setKeyboardGrabEnabled(grab);
}

bool QNitpickerPlatformWindow::setMouseGrabEnabled(bool grab)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setMouseGrabEnabled()";
	return QPlatformWindow::setMouseGrabEnabled(grab);
}

bool QNitpickerPlatformWindow::setWindowModified(bool modified)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setWindowModified()";
	return QPlatformWindow::setWindowModified(modified);
}

void QNitpickerPlatformWindow::windowEvent(QEvent *event)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::windowEvent(" << event->type() << ")";
	QPlatformWindow::windowEvent(event);
}

bool QNitpickerPlatformWindow::startSystemResize(const QPoint &pos, Qt::Corner corner)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::startSystemResize()";
	return QPlatformWindow::startSystemResize(pos, corner);
}

void QNitpickerPlatformWindow::setFrameStrutEventsEnabled(bool enabled)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::setFrameStrutEventsEnabled()";
	QPlatformWindow::setFrameStrutEventsEnabled(enabled);
}

bool QNitpickerPlatformWindow::frameStrutEventsEnabled() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::frameStrutEventsEnabled()";
	return QPlatformWindow::frameStrutEventsEnabled();
}

/* functions used by the window surface */

unsigned char *QNitpickerPlatformWindow::framebuffer()
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::framebuffer()" << _framebuffer;

	/*
	 * The new framebuffer is acquired in this function to avoid a time span when
	 * the nitpicker buffer would be black and not refilled yet by Qt.
	 */

	if (_framebuffer_changed) {

	    _framebuffer_changed = false;

		if (_framebuffer != 0)
		    Genode::env()->rm_session()->detach(_framebuffer);

		_framebuffer = Genode::env()->rm_session()->attach(_framebuffer_session.dataspace());
	}

	return _framebuffer;
}

void QNitpickerPlatformWindow::refresh(int x, int y, int w, int h)
{
	if (qnpw_verbose)
	    qDebug("QNitpickerPlatformWindow::refresh(%d, %d, %d, %d)", x, y, w, h);

	if (_geometry_changed) {

		_geometry_changed = false;

		if (window()->isVisible()) {
			QRect g(geometry());

			if (window()->transientParent()) {
				/* translate global position to parent-relative position */
				g.moveTo(window()->transientParent()->mapFromGlobal(g.topLeft()));
			}

			typedef Nitpicker::Session::Command Command;
			_nitpicker_session.enqueue<Command::Geometry>(_view_handle,
		             	 Nitpicker::Rect(Nitpicker::Point(g.x(), g.y()),
		                             	 Nitpicker::Area(g.width(), g.height())));
			_nitpicker_session.execute();
		}
	}

	_framebuffer_session.refresh(x, y, w, h);
}

EGLSurface QNitpickerPlatformWindow::egl_surface() const
{
	return _egl_surface;
}

void QNitpickerPlatformWindow::egl_surface(EGLSurface egl_surface)
{
	_egl_surface = egl_surface;
}

Nitpicker::Session_client &QNitpickerPlatformWindow::nitpicker()
{
	return _nitpicker_session;
}

Nitpicker::View_capability QNitpickerPlatformWindow::view_cap() const
{
	QNitpickerPlatformWindow *npw = const_cast<QNitpickerPlatformWindow *>(this);
	return npw->_nitpicker_session.view_capability(_view_handle);
}

QT_END_NAMESPACE
