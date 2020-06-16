/*
 * \brief  QGenodeScreen
 * \author Christian Prochaska
 * \date   2013-05-08
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#ifndef _QGENODESCREEN_H_
#define _QGENODESCREEN_H_

/* Genode includes */
#include <gui_session/connection.h>

/* Qt includes */
#include <qpa/qplatformscreen.h>

#include <QDebug>

#include "qgenodecursor.h"

QT_BEGIN_NAMESPACE

class QGenodeScreen : public QPlatformScreen
{
	private:

		Genode::Env &_env;
		QRect        _geometry;

	public:

		QGenodeScreen(Genode::Env &env) : _env(env)
		{
			Gui::Connection _gui(env);

			Framebuffer::Mode const scr_mode = _gui.mode();

			_geometry.setRect(0, 0, scr_mode.area.w(),
			                        scr_mode.area.h());
		}

		QRect geometry() const { return _geometry; }
		int depth() const { return 32; }
		QImage::Format format() const { return QImage::Format_ARGB32; }
		QDpi logicalDpi() const { return QDpi(80, 80); };

		QPlatformCursor *cursor() const
		{
			static QGenodeCursor instance(_env);
			return &instance;
		}
};

QT_END_NAMESPACE

#endif /* _QGENODESCREEN_H_ */
