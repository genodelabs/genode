/*
 * \brief  QNitpickerWindowSurface
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#ifndef _QNITPICKERWINDOWSURFACE_H_
#define _QNITPICKERWINDOWSURFACE_H_

#include <qpa/qplatformbackingstore.h>

class QNitpickerPlatformWindow;

QT_BEGIN_NAMESPACE

class QNitpickerWindowSurface : public QObject, public QPlatformBackingStore
{
	Q_OBJECT

	private:

		QNitpickerPlatformWindow *_platform_window;
		unsigned char            *_backbuffer;
		QImage                    _image;
		bool                      _framebuffer_changed;

	public:

		QNitpickerWindowSurface(QWindow *window);
		~QNitpickerWindowSurface();

		QPaintDevice *paintDevice();
		void flush(QWindow *window, const QRegion &region, const QPoint &offset);
		void resize(const QSize &size, const QRegion &staticContents);

	public slots:

		void framebuffer_changed();
};

QT_END_NAMESPACE

#endif /* _QNITPICKERWINDOWSURFACE_H_ */
