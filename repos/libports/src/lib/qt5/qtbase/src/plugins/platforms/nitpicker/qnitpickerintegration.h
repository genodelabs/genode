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


#ifndef _QNITPICKERINTEGRATION_H_
#define _QNITPICKERINTEGRATION_H_

#include <QOpenGLContext>

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformscreen.h>
#include <qpa/qplatforminputcontext.h>

#include "qnitpickerscreen.h"

QT_BEGIN_NAMESPACE

class QNitpickerIntegration : public QPlatformIntegration
{
	private:

		Genode::Env        &_env;
		QNitpickerScreen   *_nitpicker_screen;
		QScopedPointer<QPlatformInputContext> m_inputContext;

	public:

		QNitpickerIntegration(Genode::Env &env);

		void initialize() Q_DECL_OVERRIDE;
		bool hasCapability(QPlatformIntegration::Capability cap) const Q_DECL_OVERRIDE;

		QPlatformWindow *createPlatformWindow(QWindow *window) const Q_DECL_OVERRIDE;
		QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const Q_DECL_OVERRIDE;

		QAbstractEventDispatcher *createEventDispatcher() const Q_DECL_OVERRIDE;

		QPlatformFontDatabase *fontDatabase() const Q_DECL_OVERRIDE;

#ifndef QT_NO_CLIPBOARD
		QPlatformClipboard *clipboard() const Q_DECL_OVERRIDE;
#endif
		QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const Q_DECL_OVERRIDE;

                QPlatformInputContext *inputContext() const Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif /* _QNITPICKERINTEGRATION_H_ */
