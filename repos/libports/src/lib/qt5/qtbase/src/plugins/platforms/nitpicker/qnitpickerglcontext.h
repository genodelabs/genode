/*
 * \brief  QNitpickerGLContext
 * \author Christian Prochaska
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef QNITPICKERGLCONTEXT_H
#define QNITPICKERGLCONTEXT_H

#include <QOpenGLContext>

#include <qpa/qplatformopenglcontext.h>

#include <EGL/egl.h>


QT_BEGIN_NAMESPACE


class QNitpickerGLContext : public QPlatformOpenGLContext
{
	private:

		QSurfaceFormat _format;

		EGLDisplay _egl_display;
		EGLContext _egl_context;
		EGLConfig  _egl_config;

	public:

		QNitpickerGLContext(QOpenGLContext *context);

		QSurfaceFormat format() const;

		void swapBuffers(QPlatformSurface *surface);

		bool makeCurrent(QPlatformSurface *surface);
	    
		void doneCurrent();

		void (*getProcAddress(const QByteArray &procName)) ();
};

QT_END_NAMESPACE

#endif // QNITPICKERGLCONTEXT_H
