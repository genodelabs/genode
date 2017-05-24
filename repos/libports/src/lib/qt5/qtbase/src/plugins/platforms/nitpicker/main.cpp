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

/* libc includes */
#include <assert.h>

/* Qt includes */
#include "qnitpickerintegrationplugin.h"

QT_BEGIN_NAMESPACE

Genode::Env *QNitpickerIntegrationPlugin::_env = nullptr;

void initialize_qt_gui(Genode::Env &env)
{
	QNitpickerIntegrationPlugin::env(env);
}


QStringList QNitpickerIntegrationPlugin::keys() const
{
	QStringList list;
	list << "Nitpicker";
	return list;
}


QPlatformIntegration *QNitpickerIntegrationPlugin::create(const QString& system, const QStringList& paramList)
{
	Q_UNUSED(paramList);
	if (system.toLower() == "nitpicker") {
		assert(_env != nullptr);
		return new QNitpickerIntegration(*_env);
	}

	return 0;
}

Q_IMPORT_PLUGIN(QNitpickerIntegrationPlugin)

QT_END_NAMESPACE
