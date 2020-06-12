/*
 * \brief  QGenodeWindowSurface
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#ifndef _QGENODEWINDOWSURFACE_H_
#define _QGENODEWINDOWSURFACE_H_

#include <qpa/qplatformbackingstore.h>

class QGenodePlatformWindow;

QT_BEGIN_NAMESPACE

class QGenodeWindowSurface : public QObject, public QPlatformBackingStore
{
	Q_OBJECT

	private:

		QGenodePlatformWindow *_platform_window;
		unsigned char         *_backbuffer;
		QImage                 _image;
		bool                   _framebuffer_changed;

	public:

		QGenodeWindowSurface(QWindow *window);
		~QGenodeWindowSurface();

		QPaintDevice *paintDevice();
		void flush(QWindow *window, const QRegion &region, const QPoint &offset);
		void resize(const QSize &size, const QRegion &staticContents);

	public slots:

		void framebuffer_changed();
};

QT_END_NAMESPACE

#endif /* _QGENODEWINDOWSURFACE_H_ */
