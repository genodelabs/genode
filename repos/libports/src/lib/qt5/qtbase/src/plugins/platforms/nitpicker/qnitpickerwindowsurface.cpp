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

/* Genode includes */
#include <blit/blit.h>

/* Qt includes */
#include <private/qguiapplication_p.h>

#include <qpa/qplatformscreen.h>

#include "qnitpickerplatformwindow.h"

#include "qnitpickerwindowsurface.h"

#include <QDebug>

static const bool verbose = false;

QT_BEGIN_NAMESPACE

QNitpickerWindowSurface::QNitpickerWindowSurface(QWindow *window)
    : QPlatformBackingStore(window), _backbuffer(0), _framebuffer_changed(true)
{
    //qDebug() << "QNitpickerWindowSurface::QNitpickerWindowSurface:" << (long)this;

    /* Calling 'QWindow::winId()' ensures that the platform window has been created */
    window->winId();

    _platform_window = static_cast<QNitpickerPlatformWindow*>(window->handle());
    connect(_platform_window, SIGNAL(framebuffer_changed()), this, SLOT(framebuffer_changed()));
}

QNitpickerWindowSurface::~QNitpickerWindowSurface()
{
	qFree(_backbuffer);
}

QPaintDevice *QNitpickerWindowSurface::paintDevice()
{
	if (verbose)
		qDebug() << "QNitpickerWindowSurface::paintDevice()";

    if (_framebuffer_changed) {

    	_framebuffer_changed = false;
    	/*
    	 * It can happen that 'resize()' was not called yet, so the size needs
    	 * to be obtained from the window.
    	 */
        QImage::Format format = QGuiApplication::primaryScreen()->handle()->format();
        QRect geo = _platform_window->geometry();
        unsigned int const bytes_per_pixel = QGuiApplication::primaryScreen()->depth() / 8;
		qFree(_backbuffer);
		_backbuffer = (unsigned char*)qMalloc(geo.width() * geo.height() * bytes_per_pixel);
		_image = QImage(_backbuffer, geo.width(), geo.height(), geo.width() * bytes_per_pixel, format);

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
    	qDebug() << "QNitpickerWindowSurface::flush("
    	         << "window =" << window
    	         << ", region =" << region
    	         << ", offset =" << offset
    	         << ")";

	unsigned int const bytes_per_pixel = _image.depth() / 8;

	for (int i = 0; i < region.rects().size(); i++) {

		QRect rect(region.rects()[i]);

		/*
		 * It happened that after resizing a window, the given flush region was
		 * bigger than the current window size, so clipping is necessary here.
		 */

		rect &= _image.rect();

		unsigned int buffer_offset = ((rect.y() + offset.y()) * _image.bytesPerLine()) +
		                             ((rect.x() + offset.x()) * bytes_per_pixel);

		blit(_image.bits() + buffer_offset,
		     _image.bytesPerLine(),
		     _platform_window->framebuffer() + buffer_offset,
		     _image.bytesPerLine(),
		     rect.width() * bytes_per_pixel,
		     rect.height());

		_platform_window->refresh(rect.x() + offset.x(),
		                          rect.y() + offset.y(),
		                          rect.width(),
		                          rect.height());
	}

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
