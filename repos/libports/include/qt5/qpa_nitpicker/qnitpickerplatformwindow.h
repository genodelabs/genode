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
#include <qtouchdevice.h>

/* Qoost includes */
#include <qoost/qmember.h>

QT_BEGIN_NAMESPACE

class QNitpickerPlatformWindow : public QObject, public QPlatformWindow
{
	Q_OBJECT

	private:

		Genode::Env                     &_env;
		QString                          _nitpicker_session_label;
		static QStringList               _nitpicker_session_label_list;
		Nitpicker::Connection            _nitpicker_session;
		Framebuffer::Session_client      _framebuffer_session;
		unsigned char                   *_framebuffer;
		bool                             _framebuffer_changed;
		bool                             _geometry_changed;
		Framebuffer::Mode                _current_mode;
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


		typedef Genode::Codepoint Codepoint;

		struct Mapped_key
		{
			enum Event { PRESSED, RELEASED, REPEAT };

			Qt::Key   key       { Qt::Key_unknown };
			Codepoint codepoint { Codepoint::INVALID };
		};

		QHash<Input::Keycode, Qt::Key> _pressed;

		Mapped_key _map_key(Input::Keycode, Codepoint, Mapped_key::Event);
		void _key_event(Input::Keycode, Codepoint, Mapped_key::Event);
		void _mouse_button_event(Input::Keycode, bool press);

		Genode::Io_signal_handler<QNitpickerPlatformWindow> _input_signal_handler;
		Genode::Io_signal_handler<QNitpickerPlatformWindow> _mode_changed_signal_handler;

		QVector<QWindowSystemInterface::TouchPoint>  _touch_points { 16 };
		QTouchDevice                                *_touch_device;
		QTouchDevice * _init_touch_device();

		void _process_touch_events(QList<Input::Event> const &events);

		Nitpicker::Session::View_handle _create_view();
		void _adjust_and_set_geometry(const QRect &rect);

		QString _sanitize_label(QString label);

		/*
		 * Genode signals are handled as Qt signals to avoid blocking in the
		 * Genode signal handler, which could cause nested signal handler
		 * execution.
		 */

	private Q_SLOTS:

		void _handle_input();
		void _handle_mode_changed();
		
	Q_SIGNALS:

		void _input();
		void _mode_changed();

	public:

		QNitpickerPlatformWindow(Genode::Env &env, QWindow *window,
		                         int screen_width, int screen_height);

		~QNitpickerPlatformWindow();

	    QSurfaceFormat format() const override;

	    void setGeometry(const QRect &rect) override;

	    QRect geometry() const override;

	    QMargins frameMargins() const override;

	    void setVisible(bool visible) override;

	    void setWindowFlags(Qt::WindowFlags flags) override;

	    void setWindowState(Qt::WindowStates state) override;

	    WId winId() const override;

	    void setParent(const QPlatformWindow *window) override;

	    void setWindowTitle(const QString &title) override;

	    void setWindowFilePath(const QString &title) override;

	    void setWindowIcon(const QIcon &icon) override;

	    void raise() override;

	    void lower() override;

	    bool isExposed() const override;

	    bool isActive() const override;

	    bool isEmbedded() const override;

	    QPoint mapToGlobal(const QPoint &pos) const override;

	    QPoint mapFromGlobal(const QPoint &pos) const override;

	    void propagateSizeHints() override;

	    void setOpacity(qreal level) override;

	    void setMask(const QRegion &region) override;

	    void requestActivateWindow() override;

	    void handleContentOrientationChange(Qt::ScreenOrientation orientation) override;

	    qreal devicePixelRatio() const override;

	    bool setKeyboardGrabEnabled(bool grab) override;

	    bool setMouseGrabEnabled(bool grab) override;

	    bool setWindowModified(bool modified) override;

	    bool windowEvent(QEvent *event) override;

	    bool startSystemResize(const QPoint &pos, Qt::Corner corner) override;

	    void setFrameStrutEventsEnabled(bool enabled) override;

	    bool frameStrutEventsEnabled() const override;


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
