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


#ifndef _QNITPICKERPLATFORMWINDOW_H_
#define _QNITPICKERPLATFORMWINDOW_H_

/* Genode includes */
#include <input/event.h>
#include <nitpicker_session/connection.h>

/* EGL includes */
#include <EGL/egl.h>

/* Qt includes */
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtInputSupport/private/qevdevkeyboardhandler_p.h>
#include <qtouchdevice.h>

/* Qoost includes */
#include <qoost/qmember.h>

QT_BEGIN_NAMESPACE

class QNitpickerPlatformWindow : public QObject, public QPlatformWindow
{
	Q_OBJECT

	private:

		Genode::Env                     &_env;
		Nitpicker::Connection            _nitpicker_session;
		Framebuffer::Session_client      _framebuffer_session;
		unsigned char                   *_framebuffer;
		bool                             _framebuffer_changed;
		bool                             _geometry_changed;
		Framebuffer::Mode                _current_mode;
		Genode::Signal_receiver         &_signal_receiver;
		Nitpicker::Session::View_handle  _view_handle;
		Input::Session_client            _input_session;
		Genode::Attached_dataspace       _ev_buf;
		QPoint                           _mouse_position;
		Qt::KeyboardModifiers            _keyboard_modifiers;
		Qt::MouseButtons                 _mouse_button_state;
		QByteArray                       _title;
		bool                             _resize_handle;
		bool                             _decoration;
		EGLSurface                       _egl_surface;

		QPoint _local_position() const
		{
			return QPoint(_mouse_position.x() - geometry().x(),
			              _mouse_position.y() - geometry().y());
		}

		Genode::Signal_dispatcher<QNitpickerPlatformWindow> _input_signal_dispatcher;
		Genode::Signal_dispatcher<QNitpickerPlatformWindow> _mode_changed_signal_dispatcher;

		QVector<QWindowSystemInterface::TouchPoint>  _touch_points { 16 };
		QTouchDevice                                *_touch_device;
		QTouchDevice * _init_touch_device();

		void _process_touch_events(QList<Input::Event> const &events);

		Nitpicker::Session::View_handle _create_view();
		void _adjust_and_set_geometry(const QRect &rect);

	private Q_SLOTS:

		void _handle_input(unsigned int);
		void _handle_mode_changed(unsigned int);

	Q_SIGNALS:

		void _input(unsigned int);
		void _mode_changed(unsigned int);

	public:

		QNitpickerPlatformWindow(Genode::Env &env, QWindow *window,
		                         Genode::Signal_receiver &signal_receiver,
		                         int screen_width, int screen_height);

	    QWindow *window() const;

	    QPlatformWindow *parent() const;

	    QPlatformScreen *screen() const;

	    QSurfaceFormat format() const;

	    void setGeometry(const QRect &rect);

	    QRect geometry() const;

	    QMargins frameMargins() const;

	    void setVisible(bool visible);

	    void setWindowFlags(Qt::WindowFlags flags);

	    void setWindowState(Qt::WindowState state);

	    WId winId() const;

	    void setParent(const QPlatformWindow *window);

	    void setWindowTitle(const QString &title);

	    void setWindowFilePath(const QString &title);

	    void setWindowIcon(const QIcon &icon);

	    void raise();

	    void lower();

	    bool isExposed() const;

	    bool isActive() const;

	    bool isEmbedded(const QPlatformWindow *parentWindow) const;

	    QPoint mapToGlobal(const QPoint &pos) const;

	    QPoint mapFromGlobal(const QPoint &pos) const;

	    void propagateSizeHints();

	    void setOpacity(qreal level);

	    void setMask(const QRegion &region);

	    void requestActivateWindow();

	    void handleContentOrientationChange(Qt::ScreenOrientation orientation);

	    qreal devicePixelRatio() const;

	    bool setKeyboardGrabEnabled(bool grab);

	    bool setMouseGrabEnabled(bool grab);

	    bool setWindowModified(bool modified);

	    void windowEvent(QEvent *event);

	    bool startSystemResize(const QPoint &pos, Qt::Corner corner);

	    void setFrameStrutEventsEnabled(bool enabled);

	    bool frameStrutEventsEnabled() const;


	    /* for QNitpickerWindowSurface */

	    unsigned char *framebuffer();

		void refresh(int x, int y, int w, int h);


		/* for QNitpickerGLContext */ 

		EGLSurface egl_surface() const;

		void egl_surface(EGLSurface egl_surface);


		/* for QNitpickerViewWidget */

		Nitpicker::Session_client &nitpicker();
		Nitpicker::View_capability view_cap() const;

	signals:

		void framebuffer_changed();

};

QT_END_NAMESPACE

#endif /* _QNITPICKERPLATFORMWINDOW_H_ */
