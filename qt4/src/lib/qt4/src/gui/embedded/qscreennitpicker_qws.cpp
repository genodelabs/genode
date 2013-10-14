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

#ifndef QT_NO_QWS_NITPICKER

#include <util/list.h>
#include <base/sleep.h>
#include <base/printf.h>
#include <nitpicker_view/client.h>

#include <timer_session/client.h>

#include <qdebug.h>
#include <qscreen_qws.h>
#include <qwindowsystem_qws.h>
#include <private/qwindowsurface_nitpicker_qws_p.h>

#include "qinputnitpicker_qws.h"

#include "qscreennitpicker_qws.h"

/*!
    \internal

    \class QNitpickerScreen
    \ingroup qws

    \brief The QNitpickerScreen class implements a screen driver for the
    virtual framebuffer.

    Note that this class is only available in \l {Qtopia Core}.
    Custom screen drivers can be added by subclassing the
    QScreenDriverPlugin class, using the QScreenDriverFactory class to
    dynamically load the driver into the application, but there should
    only be one screen object per application.

    The Qtopia Core platform provides a \l {The Virtual
    Framebuffer}{virtual framebuffer} for development and debugging;
    the virtual framebuffer allows Qtopia Core programs to be
    developed on a desktop machine, without switching between consoles
    and X11.

    \sa QScreen, QScreenDriverPlugin, {Running Applications}
*/

/*!
    \fn bool QNitpickerScreen::connect(const QString & displaySpec)
    \reimp
*/

/*!
    \fn void QNitpickerScreen::disconnect()
    \reimp
*/

/*!
    \fn bool QNitpickerScreen::initDevice()
    \reimp
*/

/*!
    \fn void QNitpickerScreen::restore()
    \reimp
*/

/*!
    \fn void QNitpickerScreen::save()
    \reimp
*/

/*!
    \fn void QNitpickerScreen::setDirty(const QRect & r)
    \reimp
*/

/*!
    \fn void QNitpickerScreen::setMode(int nw, int nh, int nd)
    \reimp
*/

/*!
    \fn void QNitpickerScreen::shutdownDevice()
    \reimp
*/

/*!
    \fn QNitpickerScreen::QNitpickerScreen(int displayId)

    Constructs a QVNCScreen object. The \a displayId argument
    identifies the Qtopia Core server to connect to.
*/
QNitpickerScreen::QNitpickerScreen(int display_id)
    : QScreen(display_id),
    	nitpicker(0),
    	framebuffer(0),
    	inputhandler(0)
{
//	qDebug() << "QNitpickerScreen::QNitpickerScreen()";
}

/*!
    Destroys this QNitpickerScreen object.
*/
QNitpickerScreen::~QNitpickerScreen()
{
//	qDebug() << "QNitpickerScreen::~QNitpickerScreen()";
}

using namespace Genode;

QWSWindowSurface *QNitpickerScreen::createSurface(QWidget *widget) const
{
//	qDebug() << "QNitpickerScreen::createSurface()";

	QWSWindowSurface *result;
	
	if (QApplication::type() == QApplication::GuiServer) {
		result = new QWSNitpickerWindowSurface(widget, nitpicker);
	} else {
		result = 0;
	}
	
//	qDebug() << "QNitpickerScreen::createSurface() finished";
	
	return result;
}

bool QNitpickerScreen::connect(const QString &displaySpec)
{
	qDebug() << "QNitpickerScreen::connect(" << displaySpec << ")";

	/*
	 * Init sessions to the required external services
	 */
	nitpicker   = new Nitpicker::Connection();
	framebuffer = new Framebuffer::Session_client(nitpicker->framebuffer_session());
	
	Framebuffer::Mode const scr_mode = nitpicker->mode();
	int const scr_w = scr_mode.width(), scr_h = scr_mode.height();

	nitpicker->buffer(scr_mode, false);

	qDebug() << "screen is " << scr_w << "x" << scr_h;
	if (!scr_w || !scr_h) {
		qDebug("got invalid screen - spinning");
		sleep_forever();
	}

	/* set QScreen variables */
	
	data = (uchar*)env()->rm_session()->attach(framebuffer->dataspace());

	dw = w = scr_w;
	dh = h = scr_h;
	d = 16; /* FIXME */

	switch (d) {
		case 1:
			setPixelFormat(QImage::Format_Mono);
			break;
		case 8:
			setPixelFormat(QImage::Format_Indexed8);
			break;
		case 16:
			setPixelFormat(QImage::Format_RGB16);
			break;
		case 32:
			setPixelFormat(QImage::Format_ARGB32_Premultiplied);
			break;
	}

	lstep = (scr_w * d) / 8;

	const int dpi = 72;
	physWidth = qRound(dw * 25.4 / dpi);
	physHeight = qRound(dh * 25.4 / dpi);

	size = lstep * h;
	mapsize = size;

	screencols = 0;

	qDebug("Connected to Nitpicker service %s: %d x %d x %d %dx%dmm (%dx%ddpi), pixels at %p",
	  displaySpec.toLatin1().data(), w, h, d,
	  physWidth, physHeight, qRound(dw*25.4/physWidth), qRound(dh*25.4/physHeight),
	  data);
	
#if 0
	// create a view showing the full framebuffer for debugging purposes
	Capability _cap = nitpicker->create_view();
	View_client(_cap).viewport(0, 0, w, h, 0, 0, true);
	View_client(_cap).stack(Capability(), true, true);
	View_client(_cap).title("Qtopia Core Framebuffer");
#endif

#if !defined(QT_NO_QWS_MOUSE_NITPICKER) || !defined(QT_NO_QWS_KEYBOARD_NITPICKER)		

	/* connect input */

	if (qApp->type() == QApplication::GuiServer) {
		
		inputhandler = new QNitpickerInputHandler(this, nitpicker->input_session());
		
		if (!inputhandler) {
			qDebug() << "could not create input handler";
		}
	}
#endif

	qDebug() << "QNitpickerScreen::connect() finished";

	return true;
}

void QNitpickerScreen::disconnect()
{
	delete framebuffer;
	framebuffer = 0;
	delete nitpicker;
	nitpicker = 0;
	delete inputhandler;
	inputhandler = 0;
}

bool QNitpickerScreen::initDevice()
{
#ifndef QT_NO_QWS_CURSOR
	QScreenCursor::initSoftwareCursor();
#endif
	QWSServer::setBackground(QBrush(QColor(0, 0, 0)));
	return true;
}

void QNitpickerScreen::shutdownDevice()
{
}

void QNitpickerScreen::setMode(int ,int ,int)
{
	PDBG("not implemented");
}

// save the state of the graphics card
// This is needed so that e.g. we can restore the palette when switching
// between linux virtual consoles.
void QNitpickerScreen::save()
{
    // nothing to do.
}

// restore the state of the graphics card.
void QNitpickerScreen::restore()
{
}

#endif // QT_NO_QWS_NITPICKER
