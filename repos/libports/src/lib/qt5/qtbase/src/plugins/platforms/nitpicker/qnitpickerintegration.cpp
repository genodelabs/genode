/*
 * \brief  QNitpickerIntegration
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Qt includes */
#include <QtGui/private/qguiapplication_p.h>
#include "qgenodeclipboard.h"
#include "qnitpickerglcontext.h"
#include "qnitpickerintegration.h"
#include "qnitpickerplatformwindow.h"
#include "qnitpickerscreen.h"
#include "qnitpickerwindowsurface.h"
#include "qgenericunixeventdispatcher_p.h"
#include "qbasicfontdatabase_p.h"

QT_BEGIN_NAMESPACE

static const bool verbose = false;

Genode::Signal_receiver &QNitpickerIntegration::_signal_receiver()
{
	static Genode::Signal_receiver _inst;
	return _inst;
}

QNitpickerIntegration::QNitpickerIntegration()
: _signal_handler_thread(_signal_receiver()),
  _nitpicker_screen(new QNitpickerScreen()),
  _event_dispatcher(createUnixEventDispatcher())
{
    QGuiApplicationPrivate::instance()->setEventDispatcher(_event_dispatcher);
    screenAdded(_nitpicker_screen);
    _signal_handler_thread.start();
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
    return new QNitpickerPlatformWindow(window,
                                        _signal_receiver(),
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


#ifndef QT_NO_CLIPBOARD
QPlatformClipboard *QNitpickerIntegration::clipboard() const
{
	static QGenodeClipboard cb(_signal_receiver());
	return &cb;
}
#endif


QPlatformOpenGLContext *QNitpickerIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    return new QNitpickerGLContext(context);
}

QT_END_NAMESPACE
