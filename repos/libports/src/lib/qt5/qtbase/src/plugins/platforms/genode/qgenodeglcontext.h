/*
 * \brief  QGenodeGLContext
 * \author Christian Prochaska
 * \date   2013-11-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef QGENODEGLCONTEXT_H
#define QGENODEGLCONTEXT_H

#include <QOpenGLContext>

#include <qpa/qplatformopenglcontext.h>

#include <EGL/egl.h>


QT_BEGIN_NAMESPACE


class QGenodeGLContext : public QPlatformOpenGLContext
{
	private:

		QSurfaceFormat _format;

		EGLDisplay _egl_display;
		EGLContext _egl_context;
		EGLConfig  _egl_config;

	public:

		QGenodeGLContext(QOpenGLContext *context);

		QSurfaceFormat format() const Q_DECL_OVERRIDE;

		void swapBuffers(QPlatformSurface *surface) Q_DECL_OVERRIDE;

		bool makeCurrent(QPlatformSurface *surface) Q_DECL_OVERRIDE;
	    
		void doneCurrent() Q_DECL_OVERRIDE;

		QFunctionPointer getProcAddress(const char *procName) Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QGENODEGLCONTEXT_H
