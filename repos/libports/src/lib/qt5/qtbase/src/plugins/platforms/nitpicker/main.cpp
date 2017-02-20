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

#include "qnitpickerintegrationplugin.h"

QT_BEGIN_NAMESPACE

QStringList QNitpickerIntegrationPlugin::keys() const
{
    QStringList list;
    list << "Nitpicker";
    return list;
}

QPlatformIntegration *QNitpickerIntegrationPlugin::create(const QString& system, const QStringList& paramList)
{
    Q_UNUSED(paramList);
    if (system.toLower() == "nitpicker")
        return new QNitpickerIntegration;

    return 0;
}

Q_IMPORT_PLUGIN(QNitpickerIntegrationPlugin)

QT_END_NAMESPACE
