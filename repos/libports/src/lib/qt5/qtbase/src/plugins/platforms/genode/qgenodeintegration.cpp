/*
 * \brief  QGenodeIntegration
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
#include "qgenodeglcontext.h"
#include "qgenodeintegration.h"
#include "qgenodeplatformwindow.h"
#include "qgenodescreen.h"
#include "qgenodewindowsurface.h"
#include "QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h"
#include "QtFontDatabaseSupport/private/qfreetypefontdatabase_p.h"

QT_BEGIN_NAMESPACE

static const bool verbose = false;


QGenodeIntegration::QGenodeIntegration(Genode::Env &env)
: _env(env),
  _genode_screen(new QGenodeScreen(env)) { }


bool QGenodeIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
	switch (cap) {
		case ThreadedPixmaps: return true;
		default: return QPlatformIntegration::hasCapability(cap);
	}
}


QPlatformWindow *QGenodeIntegration::createPlatformWindow(QWindow *window) const
{
	if (verbose)
		qDebug() << "QGenodeIntegration::createPlatformWindow(" << window << ")";

    QRect screen_geometry = _genode_screen->geometry();
    return new QGenodePlatformWindow(_env, window,
                                     screen_geometry.width(),
                                     screen_geometry.height());
}


QPlatformBackingStore *QGenodeIntegration::createPlatformBackingStore(QWindow *window) const
{
	if (verbose)
		qDebug() << "QGenodeIntegration::createPlatformBackingStore(" << window << ")";
    return new QGenodeWindowSurface(window);
}


QAbstractEventDispatcher *QGenodeIntegration::createEventDispatcher() const
{
	if (verbose)
		qDebug() << "QGenodeIntegration::createEventDispatcher()";
	return createUnixEventDispatcher();
}


void QGenodeIntegration::initialize()
{
    QWindowSystemInterface::handleScreenAdded(_genode_screen);

    QString icStr = QPlatformInputContextFactory::requested();
    if (icStr.isNull())
        icStr = QLatin1String("compose");
    m_inputContext.reset(QPlatformInputContextFactory::create(icStr));
}


QPlatformFontDatabase *QGenodeIntegration::fontDatabase() const
{
    static QFreeTypeFontDatabase db;
    return &db;
}


#ifndef QT_NO_CLIPBOARD
QPlatformClipboard *QGenodeIntegration::clipboard() const
{
	static QGenodeClipboard cb(_env);
	return &cb;
}
#endif


QPlatformOpenGLContext *QGenodeIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    return new QGenodeGLContext(context);
}

QPlatformInputContext *QGenodeIntegration::inputContext() const
{
    return m_inputContext.data();
}

QT_END_NAMESPACE
