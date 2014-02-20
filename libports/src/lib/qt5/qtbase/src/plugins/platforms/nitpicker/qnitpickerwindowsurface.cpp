/*
 * \brief  QNitpickerWindowSurface
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <private/qguiapplication_p.h>

#include <qpa/qplatformscreen.h>

#include "qnitpickerplatformwindow.h"

#include "qnitpickerwindowsurface.h"

#include <QDebug>

static const bool verbose = false;

QT_BEGIN_NAMESPACE

QNitpickerWindowSurface::QNitpickerWindowSurface(QWindow *window)
    : QPlatformBackingStore(window), _framebuffer_changed(true)
{
    //qDebug() << "QNitpickerWindowSurface::QNitpickerWindowSurface:" << (long)this;

    _platform_window = static_cast<QNitpickerPlatformWindow*>(window->handle());
    connect(_platform_window, SIGNAL(framebuffer_changed()), this, SLOT(framebuffer_changed()));
}

QNitpickerWindowSurface::~QNitpickerWindowSurface()
{
}

QPaintDevice *QNitpickerWindowSurface::paintDevice()
{
	if (verbose)
		qDebug() << "QNitpickerWindowSurface::paintDevice()";

    if (_framebuffer_changed) {

    	if (verbose)
    		PDBG("framebuffer changed");

    	_framebuffer_changed = false;
    	/*
    	 * It can happen that 'resize()' was not called yet, so the size needs
    	 * to be obtained from the window.
    	 */
        QImage::Format format = QGuiApplication::primaryScreen()->handle()->format();
        QRect geo = _platform_window->geometry();
        _image = QImage(_platform_window->framebuffer(), geo.width(), geo.height(), 2*geo.width(), format);

        if (verbose)
        	qDebug() << "QNitpickerWindowSurface::paintDevice(): w =" << geo.width() << ", h =" << geo.height();
    }

    if (verbose)
    	qDebug() << "QNitpickerWindowSurface::paintDevice() finished";

    return &_image;
}

void QNitpickerWindowSurface::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(window);
    Q_UNUSED(region);
    Q_UNUSED(offset);

    if (verbose)
    	qDebug() << "QNitpickerWindowSurface::flush()";

    QRect geo = _platform_window->geometry();
    _platform_window->refresh(0, 0, geo.width(), geo.height());
}

void QNitpickerWindowSurface::resize(const QSize &size, const QRegion &staticContents)
{
	if (verbose)
		qDebug() << "QNitpickerWindowSurface::resize:" << this << size;
}

void QNitpickerWindowSurface::framebuffer_changed()
{
	_framebuffer_changed = true;
}

QT_END_NAMESPACE
