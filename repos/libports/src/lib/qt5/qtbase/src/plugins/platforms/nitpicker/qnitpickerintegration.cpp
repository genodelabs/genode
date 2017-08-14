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
#include <qpa/qplatforminputcontextfactory_p.h>

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

QNitpickerIntegration::QNitpickerIntegration(Genode::Env &env)
: _env(env),
  _signal_handler_thread(_signal_receiver()),
  _nitpicker_screen(new QNitpickerScreen(env))
{
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
    return new QNitpickerPlatformWindow(_env, window,
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


QAbstractEventDispatcher *QNitpickerIntegration::createEventDispatcher() const
{
	if (verbose)
		qDebug() << "QNitpickerIntegration::createEventDispatcher()";
	return createUnixEventDispatcher();
}


void QNitpickerIntegration::initialize()
{
    screenAdded(_nitpicker_screen);

    QString icStr = QPlatformInputContextFactory::requested();
    if (icStr.isNull())
        icStr = QLatin1String("compose");
    m_inputContext.reset(QPlatformInputContextFactory::create(icStr));
}


QPlatformFontDatabase *QNitpickerIntegration::fontDatabase() const
{
    static QBasicFontDatabase db;
    return &db;
}


#ifndef QT_NO_CLIPBOARD
QPlatformClipboard *QNitpickerIntegration::clipboard() const
{
	static QGenodeClipboard cb(_env, _signal_receiver());
	return &cb;
}
#endif


QPlatformOpenGLContext *QNitpickerIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    return new QNitpickerGLContext(context);
}

QPlatformInputContext *QNitpickerIntegration::inputContext() const
{
    return m_inputContext.data();
}

QT_END_NAMESPACE
