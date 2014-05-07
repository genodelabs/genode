/*
 * \brief  QNitpickerIntegration
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

#include <base/rpc_server.h>
#include <cap_session/connection.h>

/* Qt includes */
#include <QtGui/private/qguiapplication_p.h>
#include "qnitpickerglcontext.h"
#include "qnitpickerintegration.h"
#include "qnitpickerplatformwindow.h"
#include "qnitpickerscreen.h"
#include "qnitpickerwindowsurface.h"
#include "qgenericunixeventdispatcher_p.h"
#include "qbasicfontdatabase_p.h"

QT_BEGIN_NAMESPACE

static const bool verbose = false;

static Genode::Rpc_entrypoint &_entrypoint()
{
	enum { STACK_SIZE = 2*1024*sizeof(Genode::addr_t) };
	static Genode::Cap_connection cap;
	static Genode::Rpc_entrypoint entrypoint(&cap, STACK_SIZE, "qt_window_ep");
	return entrypoint;
}


QNitpickerIntegration::QNitpickerIntegration()
: _nitpicker_screen(new QNitpickerScreen()),
  _event_dispatcher(createUnixEventDispatcher())
{
    QGuiApplicationPrivate::instance()->setEventDispatcher(_event_dispatcher);
    screenAdded(_nitpicker_screen);
}


bool QNitpickerIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
	switch (cap) {
		case ThreadedPixmaps: return true;
		default: return QPlatformIntegration::hasCapability(cap);
	}
}


QPlatformWindow *QNitpickerIntegration::createPlatformWindow(QWindow *window) const
{
	if (verbose)
		qDebug() << "QNitpickerIntegration::createPlatformWindow(" << window << ")";

    QRect screen_geometry = _nitpicker_screen->geometry();
    return new QNitpickerPlatformWindow(window, _entrypoint(),
                                        screen_geometry.width(),
                                        screen_geometry.height());
}


QPlatformBackingStore *QNitpickerIntegration::createPlatformBackingStore(QWindow *window) const
{
	if (verbose)
		qDebug() << "QNitpickerIntegration::createPlatformBackingStore(" << window << ")";
    return new QNitpickerWindowSurface(window);
}


QAbstractEventDispatcher *QNitpickerIntegration::guiThreadEventDispatcher() const
{
	if (verbose)
		qDebug() << "QNitpickerIntegration::guiThreadEventDispatcher()";
	return _event_dispatcher;
}


QPlatformFontDatabase *QNitpickerIntegration::fontDatabase() const
{
    static QBasicFontDatabase db;
    return &db;
}


QPlatformOpenGLContext *QNitpickerIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    return new QNitpickerGLContext(context);
}

QT_END_NAMESPACE

