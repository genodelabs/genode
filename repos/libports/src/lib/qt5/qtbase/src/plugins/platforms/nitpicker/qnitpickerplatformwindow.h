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
#include <input/event.h>
#include <nitpicker_session/connection.h>

/* EGL includes */
#include <EGL/egl.h>

/* Qt includes */
#include <qpa/qplatformwindow.h>
#include <qevdevkeyboardhandler_p.h>

/* Qoost includes */
#include <qoost/qmember.h>

QT_BEGIN_NAMESPACE

class QNitpickerPlatformWindow : public QObject, public QPlatformWindow
{
	Q_OBJECT

	private:

		Nitpicker::Connection             _nitpicker_session;
		Framebuffer::Session_client       _framebuffer_session;
		unsigned char                    *_framebuffer;
		bool                              _framebuffer_changed;
		bool                              _geometry_changed;
		Framebuffer::Mode                 _current_mode;
		Genode::Signal_context            _mode_changed_signal_context;
		Genode::Signal_context_capability _mode_changed_signal_context_capability;
		Genode::Signal_receiver           _signal_receiver;
		Nitpicker::Session::View_handle   _view_handle;
		Input::Session_client             _input_session;
		Input::Event                     *_ev_buf;
		QMember<QTimer>                   _timer;
		Qt::MouseButtons                  _mouse_button_state;
		QEvdevKeyboardHandler             _keyboard_handler;
		QByteArray                        _title;
		bool                              _resize_handle;
		bool                              _decoration;
		EGLSurface                        _egl_surface;

		void _process_mouse_event(Input::Event *ev);
		void _process_key_event(Input::Event *ev);

		Nitpicker::Session::View_handle _create_view();
		void _adjust_and_set_geometry(const QRect &rect);

	public:

		QNitpickerPlatformWindow(QWindow *window, Genode::Rpc_entrypoint &ep,
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

		Nitpicker::View_capability view_cap() const;

	signals:

		void framebuffer_changed();

	private slots:

		void handle_events();

};

QT_END_NAMESPACE

#endif /* _QNITPICKERPLATFORMWINDOW_H_ */
