/*
 * \brief  Nitpicker QPA plugin
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _QNITPICKERINTEGRATIONPLUGIN_H_
#define _QNITPICKERINTEGRATIONPLUGIN_H_

#include <QDebug>
#include <qpa/qplatformintegrationplugin.h>
#include "qnitpickerintegration.h"

QT_BEGIN_NAMESPACE

class QNitpickerIntegrationPlugin : public QPlatformIntegrationPlugin
{
	Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QPA.QPlatformIntegrationFactoryInterface.5.1" FILE "nitpicker.json")
public:
    QStringList keys() const;
    QPlatformIntegration *create(const QString&, const QStringList&);
};

QT_END_NAMESPACE

#endif /* _QNITPICKERINTEGRATIONPLUGIN_H_ */
