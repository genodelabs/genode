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


#ifndef _QNITPICKERPLATFORMWINDOW_H_
#define _QNITPICKERPLATFORMWINDOW_H_

/* Genode includes */
#include <input/keycodes.h>

/* EGL includes */
#include <EGL/egl.h>

/* Qt includes */
#include <qpa/qplatformwindow.h>
#include <qpa/qplatformscreen.h>
#include <qpa/qwindowsysteminterface.h>
#include <QTimer>
#include <QDebug>
#include <qevdevkeyboardhandler_p.h>

/* Qoost includes */
#include <qoost/qmember.h>

#include "window_slave_policy.h"

QT_BEGIN_NAMESPACE

static const bool qnpw_verbose = false;

class QNitpickerPlatformWindow : public QObject, public QPlatformWindow
{
	Q_OBJECT

	private:

		Window_slave_policy      _window_slave_policy;
		Genode::Slave            _window_slave;
		QMember<QTimer>          _timer;
		Qt::MouseButtons         _mouse_button_state;
		QEvdevKeyboardHandler    _keyboard_handler;
		QByteArray               _title;
		bool                     _resize_handle;
		bool                     _decoration;
		EGLSurface               _egl_surface;

		void _process_mouse_event(Input::Event *ev)
		{
			QPoint local_position(ev->ax(), ev->ay());
			QPoint global_position (geometry().x() + local_position.x(),
			                        geometry().y() + local_position.y());

			//qDebug() << "local_position =" << local_position;
			//qDebug() << "global_position =" << global_position;

			switch (ev->type()) {

				case Input::Event::PRESS:

					if (qnpw_verbose)
						PDBG("PRESS");

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

					if (qnpw_verbose)
						PDBG("RELEASE");

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

					if (qnpw_verbose)
						PDBG("WHEEL");

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

		void _process_key_event(Input::Event *ev)
		{
			const bool pressed = (ev->type() == Input::Event::PRESS);
			const int keycode = ev->code();
			_keyboard_handler.processKeycode(keycode, pressed, false);
		}

	public:

		QNitpickerPlatformWindow(QWindow *window, Genode::Rpc_entrypoint &ep,
		                         int screen_width, int screen_height)
		: QPlatformWindow(window),
		  _window_slave_policy(ep, screen_width, screen_height),
		  _window_slave(ep, _window_slave_policy, 9*1024*1024),
		  _timer(this),
		  _keyboard_handler("", -1, false, false, ""),
		  _resize_handle(!window->flags().testFlag(Qt::Popup)),
		  _decoration(!window->flags().testFlag(Qt::Popup)),
		  _egl_surface(EGL_NO_SURFACE)
		{
			_window_slave_policy.wait_for_service_announcements();

			connect(_timer, SIGNAL(timeout()), this, SLOT(handle_events()));
			_timer->start(10);
		}

	    QWindow *window() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::window()";
	    	return QPlatformWindow::window();
	    }

	    QPlatformWindow *parent() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::parent()";
	    	return QPlatformWindow::parent();
	    }

	    QPlatformScreen *screen() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::screen()";
	    	return QPlatformWindow::screen();
	    }

	    QSurfaceFormat format() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::format()";
	    	return QPlatformWindow::format();
	    }

	    void setGeometry(const QRect &rect)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setGeometry(" << rect << ")";

	    	/* limit window size to screen size */
	    	QRect adjusted_rect(rect.intersected(screen()->geometry()));

	    	/* make invisible window invisible by moving it out of the screen */
	    	if (!window()->isVisible())
	    		adjusted_rect.moveRight(screen()->geometry().width());

	    	_window_slave_policy.configure(adjusted_rect.x(),
	    	                               adjusted_rect.y(),
	    	                               adjusted_rect.width(),
	    	                               adjusted_rect.height(),
	    	                               _title.constData(),
	    	                               _resize_handle, _decoration);
	    	int final_width, final_height;
	    	_window_slave_policy.size(final_width, final_height);
	    	QRect final_geometry(rect.x(), rect.y(), final_width, final_height);
	    	QPlatformWindow::setGeometry(final_geometry);

	    	emit framebuffer_changed();

	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setGeometry() finished";
	    }

	    QRect geometry() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::geometry(): returning" << QPlatformWindow::geometry();
	    	return QPlatformWindow::geometry();
	    }

	    QMargins frameMargins() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::frameMargins()";
	    	return QPlatformWindow::frameMargins();
	    }

	    void setVisible(bool visible)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setVisible(" << visible << ")";

	    	QRect g = geometry();
	    	int x = g.x();
	    	if (!visible)
	    		x += 100000;
			_window_slave_policy.configure(x, g.y(),
										   g.width(), g.height(),
										   _title.constData(),
										   _resize_handle, _decoration);

	    	emit framebuffer_changed();

	    	QPlatformWindow::setVisible(visible);

			if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setVisible() finished";
	    }

	    void setWindowFlags(Qt::WindowFlags flags)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowFlags(" << flags << ")";

	    	_resize_handle = true;
	    	_decoration = true;

	    	if (flags.testFlag(Qt::Popup)) {
	    		_resize_handle = false;
	    		_decoration = false;
	    	}

	    	QRect g = geometry();
	    	int x = g.x();
	    	if (!window()->isVisible())
	    		x += 100000;
    		_window_slave_policy.configure(x, g.y(), g.width(), g.height(),
    		                               _title.constData(), _resize_handle,
    		                               _decoration);

	    	QPlatformWindow::setWindowFlags(flags);

	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowFlags() finished";
		}

	    void setWindowState(Qt::WindowState state)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowState(" << state << ")";
	    	QPlatformWindow::setWindowState(state);
	    }

	    WId winId() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::winId()";
	    	return WId(this);
	    }

	    void setParent(const QPlatformWindow *window)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setParent()";
	    	QPlatformWindow::setParent(window);
	    }

	    void setWindowTitle(const QString &title)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowTitle(" << title << ")";

	    	QPlatformWindow::setWindowTitle(title);
	    	_title = title.toUtf8();
	    	QRect g = geometry();
	    	int x = g.x();
	    	if (!window()->isVisible())
	    		x += 100000;
	    	_window_slave_policy.configure(x, g.y(),
	    	                               g.width(), g.height(),
	    	                               _title.constData(),
	    	                               _resize_handle, _decoration);
	    	emit framebuffer_changed();

	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowTitle() finished";
	    }

	    void setWindowFilePath(const QString &title)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowFilePath(" << title << ")";
	    	QPlatformWindow::setWindowFilePath(title);
	    }

	    void setWindowIcon(const QIcon &icon)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowIcon()";
	    	QPlatformWindow::setWindowIcon(icon);
	    }

	    void raise()
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::raise()";
	    	QPlatformWindow::raise();
	    }

	    void lower()
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::lower()";
	    	QPlatformWindow::lower();
	    }

	    bool isExposed() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::isExposed()";
	    	return QPlatformWindow::isExposed();
	    }

	    bool isActive() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::isActive()";
	    	return QPlatformWindow::isActive();
	    }

	    bool isEmbedded(const QPlatformWindow *parentWindow) const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::isEmbedded()";
	    	return QPlatformWindow::isEmbedded(parentWindow);
	    }

	    QPoint mapToGlobal(const QPoint &pos) const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::mapToGlobal(" << pos << ")";
	    	return QPlatformWindow::mapToGlobal(pos);
	    }

	    QPoint mapFromGlobal(const QPoint &pos) const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::mapFromGlobal(" << pos << ")";
	    	return QPlatformWindow::mapFromGlobal(pos);
	    }

	    void propagateSizeHints()
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::propagateSizeHints()";
	    	QPlatformWindow::propagateSizeHints();
	    }

	    void setOpacity(qreal level)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setOpacity(" << level << ")";
	    	QPlatformWindow::setOpacity(level);
	    }

	    void setMask(const QRegion &region)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setMask(" << region << ")";
	    	QPlatformWindow::setMask(region);
	    }

	    void requestActivateWindow()
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::requestActivateWindow()";
	    	QPlatformWindow::requestActivateWindow();
	    }

	    void handleContentOrientationChange(Qt::ScreenOrientation orientation)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::handleContentOrientationChange()";
	    	QPlatformWindow::handleContentOrientationChange(orientation);
	    }

	    qreal devicePixelRatio() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::devicePixelRatio()";
	    	return QPlatformWindow::devicePixelRatio();
	    }

	    bool setKeyboardGrabEnabled(bool grab)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setKeyboardGrabEnabled()";
	    	return QPlatformWindow::setKeyboardGrabEnabled(grab);
	    }

	    bool setMouseGrabEnabled(bool grab)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setMouseGrabEnabled()";
	    	return QPlatformWindow::setMouseGrabEnabled(grab);
	    }

	    bool setWindowModified(bool modified)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setWindowModified()";
	    	return QPlatformWindow::setWindowModified(modified);
	    }

	    void windowEvent(QEvent *event)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::windowEvent(" << event->type() << ")";
	    	QPlatformWindow::windowEvent(event);
	    }

	    bool startSystemResize(const QPoint &pos, Qt::Corner corner)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::startSystemResize()";
	    	return QPlatformWindow::startSystemResize(pos, corner);
	    }

	    void setFrameStrutEventsEnabled(bool enabled)
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::setFrameStrutEventsEnabled()";
	    	QPlatformWindow::setFrameStrutEventsEnabled(enabled);
	    }

	    bool frameStrutEventsEnabled() const
	    {
	    	if (qnpw_verbose)
	    		qDebug() << "QNitpickerPlatformWindow::frameStrutEventsEnabled()";
	    	return QPlatformWindow::frameStrutEventsEnabled();
	    }

	    /* functions used by the window surface */

	    unsigned char *framebuffer()
	    {
	    	return _window_slave_policy.framebuffer();
	    }

		void refresh(int x, int y, int w, int h)
		{
			_window_slave_policy.refresh(x, y, w, h);
		}

		EGLSurface egl_surface() const
		{
			return _egl_surface;
		}

		void egl_surface(EGLSurface egl_surface)
		{
			_egl_surface = egl_surface;
		}

	signals:

		void framebuffer_changed();

	private slots:

		void handle_events()
		{
			/* handle framebuffer mode change events */
			if (_window_slave_policy.mode_changed()) {
				int new_width;
				int new_height;
				_window_slave_policy.size(new_width, new_height);

				if (qnpw_verbose)
					PDBG("mode change detected: %d, %d", new_width, new_height);

				QRect geo = geometry();
				geo.setWidth(new_width);
				geo.setHeight(new_height);
				QPlatformWindow::setGeometry(geo);

				if (qnpw_verbose)
					qDebug() << "calling QWindowSystemInterface::handleGeometryChange(" << geo << ")";

				QWindowSystemInterface::handleGeometryChange(window(), geo);
				emit framebuffer_changed();
			}

			/* handle input events */
			Input::Session_client input(_window_slave_policy.input_session());
			if (input.is_pending()) {
				Input::Event *ev_buf = _window_slave_policy.ev_buf();
				for (int i = 0, num_ev = input.flush(); i < num_ev; i++) {

					Input::Event *ev = &ev_buf[i];

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

					} else if (is_key_event && (ev->code() < 128)) {

						_process_key_event(ev);

					}
				}

			}


		}

};

QT_END_NAMESPACE

#endif /* _QNITPICKERPLATFORMWINDOW_H_ */
