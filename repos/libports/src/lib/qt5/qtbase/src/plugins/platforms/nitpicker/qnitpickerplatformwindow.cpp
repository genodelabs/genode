/*
 * \brief  QNitpickerPlatformWindow
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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

		i->handle_touch([&] (Input::Touch_id id, int x, int y) {

			if (id.value >= _touch_points.size()) {
				Genode::warning("drop touch input, out of bounds");
				return;
			}

			QWindowSystemInterface::TouchPoint &otp = _touch_points[id.value];
			QWindowSystemInterface::TouchPoint tp;

			tp.id   = id.value;
			tp.area = QRectF(QPointF(0, 0), QSize(1, 1));

			/* report 1x1 rectangular area centered at screen coordinates */
			tp.area.moveCenter(QPointF(x, y));

			tp.state    = otp.state == Qt::TouchPointReleased
			            ? Qt::TouchPointPressed : Qt::TouchPointMoved;
			tp.pressure = 1;

			otp = tp;
			touch_points.push_back(tp);
		});

		i->handle_touch_release([&] (Input::Touch_id id) {

			if (id.value >= _touch_points.size()) {
				Genode::warning("drop touch input, out of bounds");
				return;
			}

			QWindowSystemInterface::TouchPoint &otp = _touch_points[id.value];
			QWindowSystemInterface::TouchPoint tp;

			tp.id       = id.value;
			tp.area     = QRectF(QPointF(0, 0), QSize(1, 1));
			tp.state    = Qt::TouchPointReleased;
			tp.pressure = 0;

			otp = tp;
			touch_points.push_back(tp);
		});
	}

	QWindowSystemInterface::handleTouchEvent(0, _touch_device, touch_points);
}


static Qt::Key translate_keycode(Input::Keycode key)
{
	switch (key) {
	case Input::KEY_ENTER:     return Qt::Key_Return;
	case Input::KEY_ESC:       return Qt::Key_Escape;
	case Input::KEY_TAB:       return Qt::Key_Tab;
	case Input::KEY_BACKSPACE: return Qt::Key_Backspace;
	case Input::KEY_INSERT:    return Qt::Key_Insert;
	case Input::KEY_DELETE:    return Qt::Key_Delete;
	case Input::KEY_PRINT:     return Qt::Key_Print;
	case Input::KEY_CLEAR:     return Qt::Key_Clear;
	case Input::KEY_HOME:      return Qt::Key_Home;
	case Input::KEY_END:       return Qt::Key_End;
	case Input::KEY_LEFT:      return Qt::Key_Left;
	case Input::KEY_UP:        return Qt::Key_Up;
	case Input::KEY_RIGHT:     return Qt::Key_Right;
	case Input::KEY_DOWN:      return Qt::Key_Down;
	case Input::KEY_PAGEUP:    return Qt::Key_PageUp;
	case Input::KEY_PAGEDOWN:  return Qt::Key_PageDown;
	case Input::KEY_LEFTSHIFT: return Qt::Key_Shift;
	case Input::KEY_LEFTCTRL:  return Qt::Key_Control;
	case Input::KEY_LEFTMETA:  return Qt::Key_Meta;
	case Input::KEY_LEFTALT:   return Qt::Key_Alt;
	case Input::KEY_RIGHTALT:  return Qt::Key_AltGr;
	case Input::KEY_CAPSLOCK:  return Qt::Key_CapsLock;
	case Input::KEY_F1:        return Qt::Key_F1;
	case Input::KEY_F2:        return Qt::Key_F2;
	case Input::KEY_F3:        return Qt::Key_F3;
	case Input::KEY_F4:        return Qt::Key_F4;
	case Input::KEY_F5:        return Qt::Key_F5;
	case Input::KEY_F6:        return Qt::Key_F6;
	case Input::KEY_F7:        return Qt::Key_F7;
	case Input::KEY_F8:        return Qt::Key_F8;
	case Input::KEY_F9:        return Qt::Key_F9;
	case Input::KEY_F10:       return Qt::Key_F10;
	case Input::KEY_F11:       return Qt::Key_F11;
	case Input::KEY_F12:       return Qt::Key_F12;
	case Input::KEY_SPACE:     return Qt::Key_Space;
	case Input::KEY_0:         return Qt::Key_0;
	case Input::KEY_1:         return Qt::Key_1;
	case Input::KEY_2:         return Qt::Key_2;
	case Input::KEY_3:         return Qt::Key_3;
	case Input::KEY_4:         return Qt::Key_4;
	case Input::KEY_5:         return Qt::Key_5;
	case Input::KEY_6:         return Qt::Key_6;
	case Input::KEY_7:         return Qt::Key_7;
	case Input::KEY_8:         return Qt::Key_8;
	case Input::KEY_9:         return Qt::Key_9;
	case Input::KEY_A:         return Qt::Key_A;
	case Input::KEY_B:         return Qt::Key_B;
	case Input::KEY_C:         return Qt::Key_C;
	case Input::KEY_D:         return Qt::Key_D;
	case Input::KEY_E:         return Qt::Key_E;
	case Input::KEY_F:         return Qt::Key_F;
	case Input::KEY_G:         return Qt::Key_G;
	case Input::KEY_H:         return Qt::Key_H;
	case Input::KEY_I:         return Qt::Key_I;
	case Input::KEY_J:         return Qt::Key_J;
	case Input::KEY_K:         return Qt::Key_K;
	case Input::KEY_L:         return Qt::Key_L;
	case Input::KEY_M:         return Qt::Key_M;
	case Input::KEY_N:         return Qt::Key_N;
	case Input::KEY_O:         return Qt::Key_O;
	case Input::KEY_P:         return Qt::Key_P;
	case Input::KEY_Q:         return Qt::Key_Q;
	case Input::KEY_R:         return Qt::Key_R;
	case Input::KEY_S:         return Qt::Key_S;
	case Input::KEY_T:         return Qt::Key_T;
	case Input::KEY_U:         return Qt::Key_U;
	case Input::KEY_V:         return Qt::Key_V;
	case Input::KEY_W:         return Qt::Key_W;
	case Input::KEY_X:         return Qt::Key_X;
	case Input::KEY_Y:         return Qt::Key_Y;
	case Input::KEY_Z:         return Qt::Key_Z;
	case Input::KEY_BACK:      return Qt::Key_Back;
	case Input::KEY_FORWARD:   return Qt::Key_Forward;
	default: break;
	};
	return Qt::Key_unknown;
}


void QNitpickerPlatformWindow::_handle_input(unsigned int)
{
	QList<Input::Event> touch_events;

	_input_session.for_each_event([&] (Input::Event const &event) {

		QPoint           const orig_mouse_position     = _mouse_position;
		Qt::MouseButtons const orig_mouse_button_state = _mouse_button_state;

		event.handle_absolute_motion([&] (int x, int y) {
			_mouse_position = QPoint(x, y); });

		event.handle_press([&] (Input::Keycode key, Genode::Codepoint codepoint) {

			switch (key) {
				case Input::KEY_LEFTALT:
				case Input::KEY_RIGHTALT:   _keyboard_modifiers |= Qt::AltModifier;     break;
				case Input::KEY_LEFTCTRL:
				case Input::KEY_RIGHTCTRL:  _keyboard_modifiers |= Qt::ControlModifier; break;
				case Input::KEY_LEFTSHIFT:
				case Input::KEY_RIGHTSHIFT: _keyboard_modifiers |= Qt::ShiftModifier;   break;
				case Input::BTN_LEFT:       _mouse_button_state |= Qt::LeftButton;      break;
				case Input::BTN_RIGHT:      _mouse_button_state |= Qt::RightButton;     break;
				case Input::BTN_MIDDLE:     _mouse_button_state |= Qt::MidButton;       break;
				case Input::BTN_SIDE:       _mouse_button_state |= Qt::XButton1;        break;
				case Input::BTN_EXTRA:      _mouse_button_state |= Qt::XButton2;        break;
				default: break;
			}

			/* on mouse click, make this window the focused window */
			if (_mouse_button_state != orig_mouse_button_state)
				requestActivateWindow();

			QWindowSystemInterface::handleKeyEvent(0, QEvent::KeyPress,
			                                       translate_keycode(key),
			                                       _keyboard_modifiers,
			                                       QString() + QChar(codepoint.value));
		});

		/* handle repeat of special keys */
		event.handle_repeat([&] (Genode::Codepoint codepoint) {

			/* function-key unicodes */
			enum {
				CODEPOINT_UP        = 0xf700, CODEPOINT_DOWN     = 0xf701,
				CODEPOINT_LEFT      = 0xf702, CODEPOINT_RIGHT    = 0xf703,
				CODEPOINT_HOME      = 0xf729, CODEPOINT_INSERT   = 0xf727,
				CODEPOINT_DELETE    = 0xf728, CODEPOINT_END      = 0xf72b,
				CODEPOINT_PAGEUP    = 0xf72c, CODEPOINT_PAGEDOWN = 0xf72d,
				CODEPOINT_BACKSPACE = 8,      CODEPOINT_LINEFEED = 10,
			};

			Qt::Key repeated_key = Qt::Key_unknown;
			switch (codepoint.value) {
			case CODEPOINT_UP:        repeated_key = Qt::Key_Up;        break;
			case CODEPOINT_DOWN:      repeated_key = Qt::Key_Down;      break;
			case CODEPOINT_LEFT:      repeated_key = Qt::Key_Left;      break;
			case CODEPOINT_RIGHT:     repeated_key = Qt::Key_Right;     break;
			case CODEPOINT_HOME:      repeated_key = Qt::Key_Home;      break;
			case CODEPOINT_INSERT:    repeated_key = Qt::Key_Insert;    break;
			case CODEPOINT_DELETE:    repeated_key = Qt::Key_Delete;    break;
			case CODEPOINT_END:       repeated_key = Qt::Key_End;       break;
			case CODEPOINT_PAGEUP:    repeated_key = Qt::Key_PageUp;    break;
			case CODEPOINT_PAGEDOWN:  repeated_key = Qt::Key_PageDown;  break;
			case CODEPOINT_BACKSPACE: repeated_key = Qt::Key_Backspace; break;
			case CODEPOINT_LINEFEED:  repeated_key = Qt::Key_Return;    break;
			default: return /* from lambda */;
			};

			/*
			 * A key repeat is triggered while a key is already pressed. We
			 * respond to it by simulating a tempoary release of the key.
			 */
			QWindowSystemInterface::handleKeyEvent(0, QEvent::KeyRelease,
			                                       repeated_key,
			                                       _keyboard_modifiers,
			                                       QString());
			QWindowSystemInterface::handleKeyEvent(0, QEvent::KeyPress,
			                                       repeated_key,
			                                       _keyboard_modifiers,
			                                       QString());
		});

		event.handle_release([&] (Input::Keycode key) {

			switch (key) {
				case Input::KEY_LEFTALT:
				case Input::KEY_RIGHTALT:   _keyboard_modifiers &= ~Qt::AltModifier;     break;
				case Input::KEY_LEFTCTRL:
				case Input::KEY_RIGHTCTRL:  _keyboard_modifiers &= ~Qt::ControlModifier; break;
				case Input::KEY_LEFTSHIFT:
				case Input::KEY_RIGHTSHIFT: _keyboard_modifiers &= ~Qt::ShiftModifier;   break;
				case Input::BTN_LEFT:       _mouse_button_state &= ~Qt::LeftButton;      break;
				case Input::BTN_RIGHT:      _mouse_button_state &= ~Qt::RightButton;     break;
				case Input::BTN_MIDDLE:     _mouse_button_state &= ~Qt::MidButton;       break;
				case Input::BTN_SIDE:       _mouse_button_state &= ~Qt::XButton1;        break;
				case Input::BTN_EXTRA:      _mouse_button_state &= ~Qt::XButton2;        break;
				default: break;
			}

			QWindowSystemInterface::handleKeyEvent(0, QEvent::KeyRelease,
			                                       translate_keycode(key),
			                                       _keyboard_modifiers,
			                                       QString());
		});

		event.handle_wheel([&] (int x, int y) {
			QWindowSystemInterface::handleWheelEvent(window(),
			                                         _local_position(),
			                                         _local_position(),
			                                         y * 120,
			                                         Qt::Vertical); });

		if (_mouse_button_state != orig_mouse_button_state
		 || _mouse_position     != orig_mouse_position) {

			QWindowSystemInterface::handleMouseEvent(window(),
			                                         _local_position(),
			                                         _mouse_position,
			                                         _mouse_button_state);
		}

		if (event.touch() || event.touch_release())
			touch_events.push_back(event);
	});

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

QNitpickerPlatformWindow::QNitpickerPlatformWindow(Genode::Env &env, QWindow *window,
                                                   Genode::Signal_receiver &signal_receiver,
                                                   int screen_width, int screen_height)
: QPlatformWindow(window),
  _env(env),
  _nitpicker_session(env),
  _framebuffer_session(_nitpicker_session.framebuffer_session()),
  _framebuffer(0),
  _framebuffer_changed(false),
  _geometry_changed(false),
  _signal_receiver(signal_receiver),
  _view_handle(_create_view()),
  _input_session(env.rm(), _nitpicker_session.input_session()),
  _ev_buf(env.rm(), _input_session.dataspace()),
  _resize_handle(!window->flags().testFlag(Qt::Popup)),
  _decoration(!window->flags().testFlag(Qt::Popup)),
  _egl_surface(EGL_NO_SURFACE),
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

	if ((state == Qt::WindowMaximized) || (state == Qt::WindowFullScreen)) {
		QRect screen_geometry { screen()->geometry() };
		QWindowSystemInterface::handleGeometryChange(window(), screen_geometry);
		setGeometry(screen_geometry);
	}
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
		    _env.rm().detach(_framebuffer);

		_framebuffer = _env.rm().attach(_framebuffer_session.dataspace());
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
