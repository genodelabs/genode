/*
 * \brief  QNitpickerPlatformWindow
 * \author Christian Prochaska
 * \author Christian Helmuth
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2019 Genode Labs GmbH
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

QStringList QNitpickerPlatformWindow::_nitpicker_session_label_list;

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


static Qt::Key key_from_unicode(unsigned unicode)
{
	/* special keys: function-key unicodes */
	switch (unicode) {
	case 0x0008: return Qt::Key_Backspace;
	case 0x0009: return Qt::Key_Tab;
	case 0x000a: return Qt::Key_Return;
	case 0x001b: return Qt::Key_Escape;
	case 0xf700: return Qt::Key_Up;
	case 0xf701: return Qt::Key_Down;
	case 0xf702: return Qt::Key_Left;
	case 0xf703: return Qt::Key_Right;
	case 0xf704: return Qt::Key_F1;
	case 0xf705: return Qt::Key_F2;
	case 0xf706: return Qt::Key_F3;
	case 0xf707: return Qt::Key_F4;
	case 0xf708: return Qt::Key_F5;
	case 0xf709: return Qt::Key_F6;
	case 0xf70a: return Qt::Key_F7;
	case 0xf70b: return Qt::Key_F8;
	case 0xf70c: return Qt::Key_F9;
	case 0xf70d: return Qt::Key_F10;
	case 0xf70e: return Qt::Key_F11;
	case 0xf70f: return Qt::Key_F12;
	case 0xf727: return Qt::Key_Insert;
	case 0xf728: return Qt::Key_Delete;
	case 0xf729: return Qt::Key_Home;
	case 0xf72b: return Qt::Key_End;
	case 0xf72c: return Qt::Key_PageUp;
	case 0xf72d: return Qt::Key_PageDown;
	default: break;
	};

	/*
	 * Qt key enums are equal to the corresponding Unicode codepoints of the
	 * upper-case character.
	 */

	/* printable keys */
	if (unicode >= (unsigned)Qt::Key_Space && unicode <= (unsigned)Qt::Key_ydiaeresis)
		return Qt::Key(QChar(unicode).toUpper().unicode());

	return Qt::Key_unknown;
}


QNitpickerPlatformWindow::Mapped_key QNitpickerPlatformWindow::_map_key(Input::Keycode    key,
                                                                        Codepoint         codepoint,
                                                                        Mapped_key::Event e)
{
	/* non-printable key mappings */
	switch (key) {
	case Input::KEY_ENTER:        return Mapped_key { Qt::Key_Return };
	case Input::KEY_KPENTER:      return Mapped_key { Qt::Key_Return }; /* resolves aliasing on repeat */
	case Input::KEY_ESC:          return Mapped_key { Qt::Key_Escape };
	case Input::KEY_TAB:          return Mapped_key { Qt::Key_Tab };
	case Input::KEY_BACKSPACE:    return Mapped_key { Qt::Key_Backspace };
	case Input::KEY_INSERT:       return Mapped_key { Qt::Key_Insert };
	case Input::KEY_DELETE:       return Mapped_key { Qt::Key_Delete };
	case Input::KEY_PRINT:        return Mapped_key { Qt::Key_Print };
	case Input::KEY_CLEAR:        return Mapped_key { Qt::Key_Clear };
	case Input::KEY_HOME:         return Mapped_key { Qt::Key_Home };
	case Input::KEY_END:          return Mapped_key { Qt::Key_End };
	case Input::KEY_LEFT:         return Mapped_key { Qt::Key_Left };
	case Input::KEY_UP:           return Mapped_key { Qt::Key_Up };
	case Input::KEY_RIGHT:        return Mapped_key { Qt::Key_Right };
	case Input::KEY_DOWN:         return Mapped_key { Qt::Key_Down };
	case Input::KEY_PAGEUP:       return Mapped_key { Qt::Key_PageUp };
	case Input::KEY_PAGEDOWN:     return Mapped_key { Qt::Key_PageDown };
	case Input::KEY_LEFTSHIFT:    return Mapped_key { Qt::Key_Shift };
	case Input::KEY_RIGHTSHIFT:   return Mapped_key { Qt::Key_Shift };
	case Input::KEY_LEFTCTRL:     return Mapped_key { Qt::Key_Control };
	case Input::KEY_RIGHTCTRL:    return Mapped_key { Qt::Key_Control };
	case Input::KEY_LEFTMETA:     return Mapped_key { Qt::Key_Meta };
	case Input::KEY_RIGHTMETA:    return Mapped_key { Qt::Key_Meta };
	case Input::KEY_LEFTALT:      return Mapped_key { Qt::Key_Alt };
	case Input::KEY_RIGHTALT:     return Mapped_key { Qt::Key_AltGr };
	case Input::KEY_COMPOSE:      return Mapped_key { Qt::Key_Menu };
	case Input::KEY_CAPSLOCK:     return Mapped_key { Qt::Key_CapsLock };
	case Input::KEY_SYSRQ:        return Mapped_key { Qt::Key_SysReq };
	case Input::KEY_SCROLLLOCK:   return Mapped_key { Qt::Key_ScrollLock };
	case Input::KEY_PAUSE:        return Mapped_key { Qt::Key_Pause };
	case Input::KEY_F1:           return Mapped_key { Qt::Key_F1 };
	case Input::KEY_F2:           return Mapped_key { Qt::Key_F2 };
	case Input::KEY_F3:           return Mapped_key { Qt::Key_F3 };
	case Input::KEY_F4:           return Mapped_key { Qt::Key_F4 };
	case Input::KEY_F5:           return Mapped_key { Qt::Key_F5 };
	case Input::KEY_F6:           return Mapped_key { Qt::Key_F6 };
	case Input::KEY_F7:           return Mapped_key { Qt::Key_F7 };
	case Input::KEY_F8:           return Mapped_key { Qt::Key_F8 };
	case Input::KEY_F9:           return Mapped_key { Qt::Key_F9 };
	case Input::KEY_F10:          return Mapped_key { Qt::Key_F10 };
	case Input::KEY_F11:          return Mapped_key { Qt::Key_F11 };
	case Input::KEY_F12:          return Mapped_key { Qt::Key_F12 };
	case Input::KEY_F13:          return Mapped_key { Qt::Key_F13 };
	case Input::KEY_F14:          return Mapped_key { Qt::Key_F14 };
	case Input::KEY_F15:          return Mapped_key { Qt::Key_F15 };
	case Input::KEY_F16:          return Mapped_key { Qt::Key_F16 };
	case Input::KEY_F17:          return Mapped_key { Qt::Key_F17 };
	case Input::KEY_F18:          return Mapped_key { Qt::Key_F18 };
	case Input::KEY_F19:          return Mapped_key { Qt::Key_F19 };
	case Input::KEY_F20:          return Mapped_key { Qt::Key_F20 };
	case Input::KEY_F21:          return Mapped_key { Qt::Key_F21 };
	case Input::KEY_F22:          return Mapped_key { Qt::Key_F22 };
	case Input::KEY_F23:          return Mapped_key { Qt::Key_F23 };
	case Input::KEY_F24:          return Mapped_key { Qt::Key_F24 };
	case Input::KEY_BACK:         return Mapped_key { Qt::Key_Back };
	case Input::KEY_FORWARD:      return Mapped_key { Qt::Key_Forward };
	case Input::KEY_VOLUMEDOWN:   return Mapped_key { Qt::Key_VolumeDown };
	case Input::KEY_MUTE:         return Mapped_key { Qt::Key_VolumeMute };
	case Input::KEY_VOLUMEUP:     return Mapped_key { Qt::Key_VolumeUp };
	case Input::KEY_PREVIOUSSONG: return Mapped_key { Qt::Key_MediaPrevious };
	case Input::KEY_PLAYPAUSE:    return Mapped_key { Qt::Key_MediaTogglePlayPause };
	case Input::KEY_NEXTSONG:     return Mapped_key { Qt::Key_MediaNext };

	default: break;
	};

	/*
	 * We remember the mapping of pressed keys (but not repeated codepoints) in
	 * '_pressed' to derive the release mapping.
	 */

	switch (e) {
	case Mapped_key::PRESSED:
	case Mapped_key::REPEAT:
		{
			Qt::Key const qt_key = key_from_unicode(codepoint.value);
			if (qt_key != Qt::Key_unknown) {
				/* do not insert repeated codepoints */
				if (e == Mapped_key::PRESSED)
					_pressed.insert(key, qt_key);

				return Mapped_key { qt_key, codepoint };
			}
		} break;

	case Mapped_key::RELEASED:
		if (Qt::Key qt_key = _pressed.take(key)) {
			return Mapped_key { qt_key };
		}
		break;
	}

	/* dead keys and aborted sequences end up here */
	Genode::warning("key (", Input::key_name(key), ",", (unsigned)key,
	                ",U+", Genode::Hex((unsigned short)codepoint.value,
	                                   Genode::Hex::OMIT_PREFIX, Genode::Hex::PAD),
	                ") lacks Qt mapping");
	return Mapped_key { Qt::Key_unknown, codepoint };
}


void QNitpickerPlatformWindow::_key_event(Input::Keycode key, Codepoint codepoint,
                                          Mapped_key::Event e)
{
	bool const pressed = e != Mapped_key::RELEASED;

	Qt::KeyboardModifier current_modifier = Qt::NoModifier;

	/* FIXME ignores two keys for one modifier were pressed and only one was released */
	switch (key) {
	case Input::KEY_LEFTALT:    current_modifier = Qt::AltModifier;     break;
	case Input::KEY_LEFTCTRL:
	case Input::KEY_RIGHTCTRL:  current_modifier = Qt::ControlModifier; break;
	case Input::KEY_LEFTSHIFT:
	case Input::KEY_RIGHTSHIFT: current_modifier = Qt::ShiftModifier;   break;
	default: break;
	}

	_keyboard_modifiers.setFlag(current_modifier, pressed);

	QEvent::Type const event_type = pressed ? QEvent::KeyPress : QEvent::KeyRelease;
	Mapped_key   const mapped_key = _map_key(key, codepoint, e);
	unsigned     const unicode    = mapped_key.codepoint.valid()
	                              ? mapped_key.codepoint.value : 0;
	bool         const autorepeat = e == Mapped_key::REPEAT;

	QWindowSystemInterface::handleExtendedKeyEvent(window(),
	                                               event_type,
	                                               mapped_key.key,
	                                               _keyboard_modifiers,
	                                               key, 0, int(_keyboard_modifiers),
	                                               unicode ? QString(unicode) : QString(),
	                                               autorepeat);
}


void QNitpickerPlatformWindow::_mouse_button_event(Input::Keycode button, bool press)
{
	Qt::MouseButton current_mouse_button = Qt::NoButton;

	switch (button) {
	case Input::BTN_LEFT:    current_mouse_button = Qt::LeftButton;   break;
	case Input::BTN_RIGHT:   current_mouse_button = Qt::RightButton;  break;
	case Input::BTN_MIDDLE:  current_mouse_button = Qt::MidButton;    break;
	case Input::BTN_SIDE:    current_mouse_button = Qt::ExtraButton1; break;
	case Input::BTN_EXTRA:   current_mouse_button = Qt::ExtraButton2; break;
	case Input::BTN_FORWARD: current_mouse_button = Qt::ExtraButton3; break;
	case Input::BTN_BACK:    current_mouse_button = Qt::ExtraButton4; break;
	case Input::BTN_TASK:    current_mouse_button = Qt::ExtraButton5; break;
	default: return;
	}

	_mouse_button_state.setFlag(current_mouse_button, press);

	/* on mouse click, make this window the focused window */
	if (press) requestActivateWindow();

	QWindowSystemInterface::handleMouseEvent(window(),
	                                         _local_position(),
	                                         _mouse_position,
	                                         _mouse_button_state,
	                                         current_mouse_button,
	                                         press ? QEvent::MouseButtonPress
	                                               : QEvent::MouseButtonRelease,
	                                         _keyboard_modifiers);
}


void QNitpickerPlatformWindow::_handle_input()
{
	QList<Input::Event> touch_events;

	_input_session.for_each_event([&] (Input::Event const &event) {

		event.handle_absolute_motion([&] (int x, int y) {
			_mouse_position = QPoint(x, y);

			QWindowSystemInterface::handleMouseEvent(window(),
			                                         _local_position(),
			                                         _mouse_position,
			                                         _mouse_button_state,
			                                         Qt::NoButton,
			                                         QEvent::MouseMove,
			                                         _keyboard_modifiers);
		});

		event.handle_press([&] (Input::Keycode key, Codepoint codepoint) {
			if (key > 0 && key < 0x100)
				_key_event(key, codepoint, Mapped_key::PRESSED);
			else if (key >= Input::BTN_LEFT && key <= Input::BTN_TASK)
				_mouse_button_event(key, true);
		});

		event.handle_release([&] (Input::Keycode key) {
			if (key > 0 && key < 0x100)
				_key_event(key, Codepoint { Codepoint::INVALID }, Mapped_key::RELEASED);
			else if (key >= Input::BTN_LEFT && key <= Input::BTN_TASK)
				_mouse_button_event(key, false);
		});

		event.handle_repeat([&] (Codepoint codepoint) {
			_key_event(Input::KEY_UNKNOWN, codepoint, Mapped_key::REPEAT);
		});

		event.handle_wheel([&] (int x, int y) {
			QWindowSystemInterface::handleWheelEvent(window(),
			                                         _local_position(),
			                                         _local_position(),
			                                         QPoint(),
			                                         QPoint(0, y * 120),
			                                         _keyboard_modifiers); });

		if (event.touch() || event.touch_release())
			touch_events.push_back(event);
	});

	/* process all gathered touch events */
	_process_touch_events(touch_events);
}


void QNitpickerPlatformWindow::_handle_mode_changed()
{
	Framebuffer::Mode mode(_nitpicker_session.mode());

	if ((mode.width() == 0) && (mode.height() == 0)) {
		/* interpret a size of 0x0 as indication to close the window */
		QWindowSystemInterface::handleCloseEvent(window(), 0);
		/* don't actually set geometry to 0x0; either close or remain open */
		return;
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


QString QNitpickerPlatformWindow::_sanitize_label(QString label)
{
	enum { MAX_LABEL = 25 };

	/* remove any occurences of '"' */
	label.remove("\"");

	/* truncate label and append '..' */
	if (label.length() > MAX_LABEL) {
		label.truncate(MAX_LABEL - 2);
		label.append("..");
	}

	/* Make sure that the window is distinguishable by the layouter */
	if (label.isEmpty())
		label = QString("Untitled Window");

	if (_nitpicker_session_label_list.contains(label))
		for (unsigned int i = 2; ; i++) {
			QString versioned_label = label + "." + QString::number(i);
			if (!_nitpicker_session_label_list.contains(versioned_label)) {
				label = versioned_label;
				break;
			}
		}

	return label;
}


QNitpickerPlatformWindow::QNitpickerPlatformWindow(Genode::Env &env, QWindow *window,
                                                   int screen_width, int screen_height)
: QPlatformWindow(window),
  _env(env),
  _nitpicker_session_label(_sanitize_label(window->title())),
  _nitpicker_session(env, _nitpicker_session_label.toStdString().c_str()),
  _framebuffer_session(_nitpicker_session.framebuffer_session()),
  _framebuffer(0),
  _framebuffer_changed(false),
  _geometry_changed(false),
  _view_handle(_create_view()),
  _input_session(env.rm(), _nitpicker_session.input_session()),
  _ev_buf(env.rm(), _input_session.dataspace()),
  _resize_handle(!window->flags().testFlag(Qt::Popup)),
  _decoration(!window->flags().testFlag(Qt::Popup)),
  _egl_surface(EGL_NO_SURFACE),
  _input_signal_handler(_env.ep(), *this,
                        &QNitpickerPlatformWindow::_input),
  _mode_changed_signal_handler(_env.ep(), *this,
                               &QNitpickerPlatformWindow::_mode_changed),
  _touch_device(_init_touch_device())
{
	if (qnpw_verbose)
		if (window->transientParent())
			qDebug() << "QNitpickerPlatformWindow(): child window of" << window->transientParent();

	_nitpicker_session_label_list.append(_nitpicker_session_label);

	_input_session.sigh(_input_signal_handler);

	_nitpicker_session.mode_sigh(_mode_changed_signal_handler);

	_adjust_and_set_geometry(geometry());

	if (_view_handle.valid()) {

		/* bring the view to the top */
		typedef Nitpicker::Session::Command Command;
		_nitpicker_session.enqueue<Command::To_front>(_view_handle);
		_nitpicker_session.execute();
	}

	connect(this, SIGNAL(_input()),
	        this, SLOT(_handle_input()),
	        Qt::QueuedConnection);

	connect(this, SIGNAL(_mode_changed()),
	        this, SLOT(_handle_mode_changed()),
	        Qt::QueuedConnection);
}

QNitpickerPlatformWindow::~QNitpickerPlatformWindow()
{
	_nitpicker_session_label_list.removeOne(_nitpicker_session_label);
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

void QNitpickerPlatformWindow::setWindowState(Qt::WindowStates state)
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

bool QNitpickerPlatformWindow::isEmbedded() const
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::isEmbedded()";
	return QPlatformWindow::isEmbedded();
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

bool QNitpickerPlatformWindow::windowEvent(QEvent *event)
{
	if (qnpw_verbose)
	    qDebug() << "QNitpickerPlatformWindow::windowEvent(" << event->type() << ")";
	return QPlatformWindow::windowEvent(event);
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
