/**************************************************************************** 
** 
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies). 
** Contact: Qt Software Information (qt-info@nokia.com) 
** 
** This file is part of the QtCore module of the Qt Toolkit. 
** 
** $QT_BEGIN_LICENSE:LGPL$ 
** Commercial Usage 
** Licensees holding valid Qt Commercial licenses may use this file in 
** accordance with the Qt Commercial License Agreement provided with the 
** Software or, alternatively, in accordance with the terms contained in 
** a written agreement between you and Nokia. 
** 
** GNU Lesser General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU Lesser 
** General Public License version 2.1 as published by the Free Software 
** Foundation and appearing in the file LICENSE.LGPL included in the 
** packaging of this file.  Please review the following information to 
** ensure the GNU Lesser General Public License version 2.1 requirements 
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html. 
** 
** In addition, as a special exception, Nokia gives you certain 
** additional rights. These rights are described in the Nokia Qt LGPL 
** Exception version 1.0, included in the file LGPL_EXCEPTION.txt in this 
** package. 
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU 
** General Public License version 3.0 as published by the Free Software 
** Foundation and appearing in the file LICENSE.GPL included in the 
** packaging of this file.  Please review the following information to 
** ensure the GNU General Public License version 3.0 requirements will be 
** met: http://www.gnu.org/copyleft/gpl.html. 
** 
** If you are unsure which license is appropriate for your use, please 
** contact the sales department at qt-sales@nokia.com. 
** $QT_END_LICENSE$ 
** 
****************************************************************************/

#include <base/env.h>
#include <framebuffer_session/client.h>
#include <nitpicker_view/client.h>

#include <qdebug.h>

#include "qwindowsystem_qws.h" // class QWSWindow

#include "qwindowsurface_nitpicker_qws_p.h"

QWSNitpickerWindowSurface::QWSNitpickerWindowSurface(QWidget *widget, Nitpicker::Session_client *nitpicker)
    : QWSLocalMemSurface(widget),
      _nitpicker(nitpicker),
      moving(false)
{
//	qDebug() << "QWSNitpickerWindowSurface(widget, _nitpicker)";
	
	_view_cap = _nitpicker->create_view();
	_view_client = new Nitpicker::View_client(_view_cap);
	
	_framebuffer_session_client = new Framebuffer::Session_client(_nitpicker->framebuffer_session());

	connect(qwsServer, SIGNAL(windowEvent(QWSWindow*, QWSServer::WindowEvent)),
	        this, SLOT(windowEvent(QWSWindow*, QWSServer::WindowEvent)));
}

QWSNitpickerWindowSurface::~QWSNitpickerWindowSurface()
{
//	qDebug() << "~QWSNitpickerWindowSurface()";
	_nitpicker->destroy_view(_view_cap);
	delete _view_client;
	delete _framebuffer_session_client;
}

bool QWSNitpickerWindowSurface::move(const QPoint &offset)
{
//	qDebug() << "QWSNitpickerWindowSurface::move(" << offset << ")";

	moving = QWSLocalMemSurface::move(offset); 
	
//	qDebug() << "QWSNitpickerWindowSurface::move() finished";

	return moving;
}

void QWSNitpickerWindowSurface::flush(QWidget *widget, const QRegion &region,
                                      const QPoint &offset)
{
//	qDebug() << "QWSNitpickerWindowSurface::flush()";
	
	QWSLocalMemSurface::flush(widget, region, offset);

	/* refresh the view */
	QRect rect = geometry();
    _framebuffer_session_client->refresh(rect.x(), rect.y(),
                                         rect.width(), rect.height());

//	qDebug() << "QWSNitpickerWindowSurface::flush() finished";
}

void QWSNitpickerWindowSurface::windowEvent(QWSWindow *window,
                                            QWSServer::WindowEvent eventType)
{
	if (window->winId() == winId()) {
		
		switch (eventType) {
		
			case QWSServer::Geometry:
			{
//				qDebug() << "QWSServer::Geometry";

				QRect rect = geometry();

				if (moving) {
					moving = false;
					/* flush() does not get called while moving, so update the viewport with refresh */
					_view_client->viewport(rect.x(), rect.y(),
										   rect.width(), rect.height(),
										   -rect.x(), -rect.y(), true);
				} else {
					_view_client->viewport(rect.x(), rect.y(),
										   rect.width(), rect.height(),
										   -rect.x(), -rect.y(), false);
				}

				break;
			}
			
			case QWSServer::Show:
			{
//				qDebug() << "QWSServer::Show";
				/* show the view */
				QRect rect = geometry();
				_view_client->viewport(rect.x(), rect.y(),
				                       rect.width(), rect.height(),
				                       -rect.x(), -rect.y(), false);
				break;
			}
			
			case QWSServer::Hide:
			{
//				qDebug() << "QWSServer::Hide";
				/* hide the view */
				_view_client->viewport(0, 0,
				                       0, 0,
				                       0, 0, true);
			}
			
			case QWSServer::Raise:
			{
//				qDebug() << "QWSServer::Raise";
				/* bring view to front */
				_view_client->stack(Nitpicker::View_capability(), true, true);
				break;
			}
			
			case QWSServer::Name:
			{
//				qDebug() << "QWSServer::Name";
				/* update view title */
				_view_client->title(window->name().toAscii().constData());
				break;
			}
			default:
				break;
		}
	}
}
