/****************************************************************************
**
** Copyright (C) 1992-2007 Trolltech ASA. All rights reserved.
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://trolltech.com/products/qt/licenses/licensing/opensource/
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://trolltech.com/products/qt/licenses/licensing/licensingoverview
** or contact the sales department at sales@trolltech.com.
**
** In addition, as a special exception, Trolltech gives you certain
** additional rights. These rights are described in the Trolltech GPL
** Exception version 1.0, which can be found at
** http://www.trolltech.com/products/qt/gplexception/ and in the file
** GPL_EXCEPTION.txt in this package.
**
** In addition, as a special exception, Trolltech, as the sole copyright
** holder for Qt Designer, grants users of the Qt/Eclipse Integration
** plug-in the right for the Qt/Eclipse Integration to link to
** functionality provided by Qt Designer and its related libraries.
**
** Trolltech reserves all rights not expressly granted herein.
** 
** Trolltech ASA (c) 2007
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#ifndef QWINDOWSURFACE_NITPICKER_QWS_P_H
#define QWINDOWSURFACE_NITPICKER_QWS_P_H

#include <base/env.h>
#include <framebuffer_session/client.h>
#include <nitpicker_session/client.h>
#include <nitpicker_view/client.h>

#include <QWSServer>

#include <private/qwindowsurface_qws_p.h>

class QWSWindow;

class Q_GUI_EXPORT QWSNitpickerWindowSurface : public QObject, public QWSLocalMemSurface
{
	Q_OBJECT
	
public:
    QWSNitpickerWindowSurface(QWidget *widget, Nitpicker::Session_client *nitpicker);
    ~QWSNitpickerWindowSurface();

    virtual bool move(const QPoint &offset);

    virtual void flush(QWidget *widget, const QRegion &region,
                       const QPoint &offset);

    Nitpicker::View_capability view_cap() { return _view_cap; }

private slots:
	void windowEvent (QWSWindow *window, QWSServer::WindowEvent eventType);

private:
	Nitpicker::Session_client  *_nitpicker;
	Nitpicker::View_capability  _view_cap;
	Nitpicker::View_client     *_view_client;
	Framebuffer::Session_client *_framebuffer_session_client;
	
	bool moving;
};

#endif // QWINDOWSURFACE_NITPICKER_QWS_P_H
