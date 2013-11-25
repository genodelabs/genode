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


#ifndef _QNITPICKERINTEGRATION_H_
#define _QNITPICKERINTEGRATION_H_

#include <QOpenGLContext>

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformscreen.h>

#include "qnitpickerscreen.h"

QT_BEGIN_NAMESPACE

class QNitpickerIntegration : public QPlatformIntegration
{
	private:

		QNitpickerScreen         *_nitpicker_screen;
	    QAbstractEventDispatcher *_event_dispatcher;

	public:

		QNitpickerIntegration();

		bool hasCapability(QPlatformIntegration::Capability cap) const;

		QPlatformWindow *createPlatformWindow(QWindow *window) const;
		QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const;
	    QAbstractEventDispatcher *guiThreadEventDispatcher() const;

		QPlatformFontDatabase *fontDatabase() const;

		QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;
};

QT_END_NAMESPACE

#endif /* _QNITPICKERINTEGRATION_H_ */
