/*
 * \brief  QGenodePlatformWindow
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
#include <gui_session/client.h>

/* Qt includes */
#include <qpa/qplatformscreen.h>
#include <QGuiApplication>
#include <QDebug>

#include "qgenodeplatformwindow.h"

QT_BEGIN_NAMESPACE

static const bool qnpw_verbose = false/*true*/;

QStringList QGenodePlatformWindow::_gui_session_label_list;

QTouchDevice * QGenodePlatformWindow::_init_touch_device()
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

void QGenodePlatformWindow::_process_touch_events(QList<Input::Event> const &events)
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


QGenodePlatformWindow::Mapped_key QGenodePlatformWindow::_map_key(Input::Keycode    key,
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


void QGenodePlatformWindow::_key_event(Input::Keycode key, Codepoint codepoint,
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


void QGenodePlatformWindow::_mouse_button_event(Input::Keycode button, bool press)
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


void QGenodePlatformWindow::_handle_input()
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


void QGenodePlatformWindow::_handle_mode_changed()
{
	Framebuffer::Mode mode(_gui_session.mode());

	if ((mode.area.w() == 0) && (mode.area.h() == 0)) {
		/* interpret a size of 0x0 as indication to close the window */
		QWindowSystemInterface::handleCloseEvent(window(), 0);
		/* don't actually set geometry to 0x0; either close or remain open */
		return;
	}

	if (mode.area != _current_mode.area) {

		QRect geo(geometry());
		geo.setWidth (mode.area.w());
		geo.setHeight(mode.area.h());

		QWindowSystemInterface::handleGeometryChange(window(), geo);

		setGeometry(geo);
	}
}


Gui::Session::View_handle QGenodePlatformWindow::_create_view()
{
	if (window()->type() == Qt::Desktop)
		return Gui::Session::View_handle();

	if (window()->type() == Qt::Dialog)
		return _gui_session.create_view();

	/*
	 * Popup menus should never get a window decoration, therefore we set a top
	 * level Qt window as 'transient parent'.
	 */
	if (!window()->transientParent() && (window()->type() == Qt::Popup)) {
		QWindow *top_window = QGuiApplication::topLevelWindows().first();
	    window()->setTransientParent(top_window);
	}

	if (window()->transientParent()) {
		QGenodePlatformWindow *parent_platform_window =
			static_cast<QGenodePlatformWindow*>(window()->transientParent()->handle());

		Gui::Session::View_handle parent_handle =
			_gui_session.view_handle(parent_platform_window->view_cap());

		Gui::Session::View_handle result =
			_gui_session.create_view(parent_handle);

		_gui_session.release_view_handle(parent_handle);

		return result;
	}

	return _gui_session.create_view();
}

void QGenodePlatformWindow::_adjust_and_set_geometry(const QRect &rect)
{
	/* limit window size to screen size */
	QRect adjusted_rect(rect.intersected(screen()->geometry()));

	/* Currently, top level windows must start at (0,0) */
	if (!window()->transientParent())
		adjusted_rect.moveTo(0, 0);

	QPlatformWindow::setGeometry(adjusted_rect);

	Framebuffer::Mode const mode { .area = { (unsigned)adjusted_rect.width(),
	                                         (unsigned)adjusted_rect.height() } };
	_gui_session.buffer(mode, false);

	_current_mode = mode;

	_framebuffer_changed = true;
	_geometry_changed = true;

	emit framebuffer_changed();
}


QString QGenodePlatformWindow::_sanitize_label(QString label)
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

	if (_gui_session_label_list.contains(label))
		for (unsigned int i = 2; ; i++) {
			QString versioned_label = label + "." + QString::number(i);
			if (!_gui_session_label_list.contains(versioned_label)) {
				label = versioned_label;
				break;
			}
		}

	return label;
}


QGenodePlatformWindow::QGenodePlatformWindow(Genode::Env &env, QWindow *window,
                                             int screen_width, int screen_height)
: QPlatformWindow(window),
  _env(env),
  _gui_session_label(_sanitize_label(window->title())),
  _gui_session(env, _gui_session_label.toStdString().c_str()),
  _framebuffer_session(_gui_session.framebuffer_session()),
  _framebuffer(0),
  _framebuffer_changed(false),
  _geometry_changed(false),
  _view_handle(_create_view()),
  _input_session(env.rm(), _gui_session.input_session()),
  _ev_buf(env.rm(), _input_session.dataspace()),
  _resize_handle(!window->flags().testFlag(Qt::Popup)),
  _decoration(!window->flags().testFlag(Qt::Popup)),
  _egl_surface(EGL_NO_SURFACE),
  _input_signal_handler(_env.ep(), *this,
                        &QGenodePlatformWindow::_input),
  _mode_changed_signal_handler(_env.ep(), *this,
                               &QGenodePlatformWindow::_mode_changed),
  _touch_device(_init_touch_device())
{
	if (qnpw_verbose)
		if (window->transientParent())
			qDebug() << "QGenodePlatformWindow(): child window of" << window->transientParent();

	_gui_session_label_list.append(_gui_session_label);

	_input_session.sigh(_input_signal_handler);

	_gui_session.mode_sigh(_mode_changed_signal_handler);

	_adjust_and_set_geometry(geometry());

	if (_view_handle.valid()) {

		/* bring the view to the top */
		typedef Gui::Session::Command Command;
		_gui_session.enqueue<Command::To_front>(_view_handle);
		_gui_session.execute();
	}

	connect(this, SIGNAL(_input()),
	        this, SLOT(_handle_input()),
	        Qt::QueuedConnection);

	connect(this, SIGNAL(_mode_changed()),
	        this, SLOT(_handle_mode_changed()),
	        Qt::QueuedConnection);
}

QGenodePlatformWindow::~QGenodePlatformWindow()
{
	_gui_session_label_list.removeOne(_gui_session_label);
}

QSurfaceFormat QGenodePlatformWindow::format() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::format()";
	return QPlatformWindow::format();
}

void QGenodePlatformWindow::setGeometry(const QRect &rect)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setGeometry(" << rect << ")";

	_adjust_and_set_geometry(rect);

	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setGeometry() finished";
}

QRect QGenodePlatformWindow::geometry() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::geometry(): returning" << QPlatformWindow::geometry();
	return QPlatformWindow::geometry();
}

QMargins QGenodePlatformWindow::frameMargins() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::frameMargins()";
	return QPlatformWindow::frameMargins();
}

void QGenodePlatformWindow::setVisible(bool visible)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setVisible(" << visible << ")";

	typedef Gui::Session::Command Command;

	if (visible) {
		QRect g(geometry());

		if (window()->transientParent()) {
			/* translate global position to parent-relative position */
			g.moveTo(window()->transientParent()->mapFromGlobal(g.topLeft()));
		}

		_gui_session.enqueue<Command::Geometry>(_view_handle,
			Gui::Rect(Gui::Point(g.x(), g.y()),
			Gui::Area(g.width(), g.height())));
	} else {

		_gui_session.enqueue<Command::Geometry>(_view_handle,
		     Gui::Rect(Gui::Point(), Gui::Area(0, 0)));
	}

	_gui_session.execute();



	QPlatformWindow::setVisible(visible);

	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setVisible() finished";
}

void QGenodePlatformWindow::setWindowFlags(Qt::WindowFlags flags)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowFlags(" << flags << ")";

	QPlatformWindow::setWindowFlags(flags);

	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowFlags() finished";
}

void QGenodePlatformWindow::setWindowState(Qt::WindowStates state)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowState(" << state << ")";

	QPlatformWindow::setWindowState(state);

	if ((state == Qt::WindowMaximized) || (state == Qt::WindowFullScreen)) {
		QRect screen_geometry { screen()->geometry() };
		QWindowSystemInterface::handleGeometryChange(window(), screen_geometry);
		setGeometry(screen_geometry);
	}
}

WId QGenodePlatformWindow::winId() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::winId()";
	return WId(this);
}

void QGenodePlatformWindow::setParent(const QPlatformWindow *window)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setParent()";
	QPlatformWindow::setParent(window);
}

void QGenodePlatformWindow::setWindowTitle(const QString &title)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowTitle(" << title << ")";

	QPlatformWindow::setWindowTitle(title);

	_title = title.toLocal8Bit();

	typedef Gui::Session::Command Command;

	if (_view_handle.valid()) {
		_gui_session.enqueue<Command::Title>(_view_handle, _title.constData());
		_gui_session.execute();
	}

	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowTitle() finished";
}

void QGenodePlatformWindow::setWindowFilePath(const QString &title)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowFilePath(" << title << ")";
	QPlatformWindow::setWindowFilePath(title);
}

void QGenodePlatformWindow::setWindowIcon(const QIcon &icon)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowIcon()";
	QPlatformWindow::setWindowIcon(icon);
}

void QGenodePlatformWindow::raise()
{
	/* bring the view to the top */
	_gui_session.enqueue<Gui::Session::Command::To_front>(_view_handle);
	_gui_session.execute();

	QPlatformWindow::raise();
}

void QGenodePlatformWindow::lower()
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::lower()";
	QPlatformWindow::lower();
}

bool QGenodePlatformWindow::isExposed() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::isExposed()";
	return QPlatformWindow::isExposed();
}

bool QGenodePlatformWindow::isActive() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::isActive()";
	return QPlatformWindow::isActive();
}

bool QGenodePlatformWindow::isEmbedded() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::isEmbedded()";
	return QPlatformWindow::isEmbedded();
}

QPoint QGenodePlatformWindow::mapToGlobal(const QPoint &pos) const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::mapToGlobal(" << pos << ")";
	return QPlatformWindow::mapToGlobal(pos);
}

QPoint QGenodePlatformWindow::mapFromGlobal(const QPoint &pos) const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::mapFromGlobal(" << pos << ")";
	return QPlatformWindow::mapFromGlobal(pos);
}

void QGenodePlatformWindow::propagateSizeHints()
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::propagateSizeHints()";
	QPlatformWindow::propagateSizeHints();
}

void QGenodePlatformWindow::setOpacity(qreal level)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setOpacity(" << level << ")";
	QPlatformWindow::setOpacity(level);
}

void QGenodePlatformWindow::setMask(const QRegion &region)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setMask(" << region << ")";
	QPlatformWindow::setMask(region);
}

void QGenodePlatformWindow::requestActivateWindow()
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::requestActivateWindow()";
	QPlatformWindow::requestActivateWindow();
}

void QGenodePlatformWindow::handleContentOrientationChange(Qt::ScreenOrientation orientation)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::handleContentOrientationChange()";
	QPlatformWindow::handleContentOrientationChange(orientation);
}

qreal QGenodePlatformWindow::devicePixelRatio() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::devicePixelRatio()";
	return QPlatformWindow::devicePixelRatio();
}

bool QGenodePlatformWindow::setKeyboardGrabEnabled(bool grab)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setKeyboardGrabEnabled()";
	return QPlatformWindow::setKeyboardGrabEnabled(grab);
}

bool QGenodePlatformWindow::setMouseGrabEnabled(bool grab)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setMouseGrabEnabled()";
	return QPlatformWindow::setMouseGrabEnabled(grab);
}

bool QGenodePlatformWindow::setWindowModified(bool modified)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setWindowModified()";
	return QPlatformWindow::setWindowModified(modified);
}

bool QGenodePlatformWindow::windowEvent(QEvent *event)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::windowEvent(" << event->type() << ")";
	return QPlatformWindow::windowEvent(event);
}

bool QGenodePlatformWindow::startSystemResize(const QPoint &pos, Qt::Corner corner)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::startSystemResize()";
	return QPlatformWindow::startSystemResize(pos, corner);
}

void QGenodePlatformWindow::setFrameStrutEventsEnabled(bool enabled)
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::setFrameStrutEventsEnabled()";
	QPlatformWindow::setFrameStrutEventsEnabled(enabled);
}

bool QGenodePlatformWindow::frameStrutEventsEnabled() const
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::frameStrutEventsEnabled()";
	return QPlatformWindow::frameStrutEventsEnabled();
}

/* functions used by the window surface */

unsigned char *QGenodePlatformWindow::framebuffer()
{
	if (qnpw_verbose)
	    qDebug() << "QGenodePlatformWindow::framebuffer()" << _framebuffer;

	/*
	 * The new framebuffer is acquired in this function to avoid a time span when
	 * the GUI buffer would be black and not refilled yet by Qt.
	 */

	if (_framebuffer_changed) {

	    _framebuffer_changed = false;

		if (_framebuffer != 0)
		    _env.rm().detach(_framebuffer);

		_framebuffer = _env.rm().attach(_framebuffer_session.dataspace());
	}

	return _framebuffer;
}

void QGenodePlatformWindow::refresh(int x, int y, int w, int h)
{
	if (qnpw_verbose)
	    qDebug("QGenodePlatformWindow::refresh(%d, %d, %d, %d)", x, y, w, h);

	if (_geometry_changed) {

		_geometry_changed = false;

		if (window()->isVisible()) {
			QRect g(geometry());

			if (window()->transientParent()) {
				/* translate global position to parent-relative position */
				g.moveTo(window()->transientParent()->mapFromGlobal(g.topLeft()));
			}

			typedef Gui::Session::Command Command;
			_gui_session.enqueue<Command::Geometry>(_view_handle,
		             	 Gui::Rect(Gui::Point(g.x(), g.y()),
		                             	 Gui::Area(g.width(), g.height())));
			_gui_session.execute();
		}
	}

	_framebuffer_session.refresh(x, y, w, h);
}

EGLSurface QGenodePlatformWindow::egl_surface() const
{
	return _egl_surface;
}

void QGenodePlatformWindow::egl_surface(EGLSurface egl_surface)
{
	_egl_surface = egl_surface;
}

Gui::Session_client &QGenodePlatformWindow::gui_session()
{
	return _gui_session;
}

Gui::View_capability QGenodePlatformWindow::view_cap() const
{
	QGenodePlatformWindow *npw = const_cast<QGenodePlatformWindow *>(this);
	return npw->_gui_session.view_capability(_view_handle);
}

QT_END_NAMESPACE
