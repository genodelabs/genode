#include <base/env.h>
#include <input/keycodes.h>

#include <QTimer>
#include <qdebug.h>

#include <qwindowsystem_qws.h>
#include <qmousedriverfactory_qws.h>
#include <qkbddriverfactory_qws.h>

#include "qinputnitpicker_qws.h"

#if !defined(QT_NO_QWS_MOUSE_NITPICKER) || !defined(QT_NO_QWS_KEYBOARD_NITPICKER)

using namespace Genode;

QNitpickerInputHandler::QNitpickerInputHandler(QScreen *screen,
                                               Input::Session_capability input_session_cap,
                                               const QString &driver,
                                               const QString &device)
{
	input = new Input::Session_client(input_session_cap);

	ev_buf = static_cast<Input::Event *>
	         (env()->rm_session()->attach(input->dataspace()));

	qDebug() << "QNitpickerInputHandler: input buffer at " << ev_buf;

#ifndef QT_NO_QWS_MOUSE_NITPICKER
	mouse = new QNitpickerMouseHandler();
	qwsServer->setDefaultMouse("None");
#endif

#ifndef QT_NO_QWS_KEYBOARD_NITPICKER
	keyboard = new QNitpickerKeyboardHandler();
	qwsServer->setDefaultKeyboard("None");
#endif

	setScreen(screen);

	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(readInputData()));
	timer->start(10);
}

QNitpickerInputHandler::~QNitpickerInputHandler()
{
	delete timer;
	delete input;

#ifndef QT_NO_QWS_MOUSE_NITPICKER
	delete mouse;
#endif

#ifndef QT_NO_QWS_KEYBOARD_NITPICKER
	delete keyboard;
#endif
}

void QNitpickerInputHandler::setScreen(const QScreen *screen)
{
#ifndef QT_NO_QWS_MOUSE_NITPICKER
	if (mouse) {
		mouse->setScreen(screen);
	}
#endif
}

void QNitpickerInputHandler::readInputData()
{
//	qDebug() << "QNitpickerInputHandler::readInputData()";

	int i, num_ev;

	if (input->is_pending()) {
		for (i = 0, num_ev = input->flush(); i < num_ev; i++) {

			Input::Event *ev = &ev_buf[i];

			bool const is_key_event = ev->type() == Input::Event::PRESS
			                       || ev->type() == Input::Event::RELEASE;

			bool const is_mouse_button_event =
				is_key_event && (ev->code() == Input::BTN_LEFT
				              || ev->code() == Input::BTN_MIDDLE
				              || ev->code() == Input::BTN_RIGHT);

			if (ev->type() == Input::Event::MOTION
			 || ev->type() == Input::Event::WHEEL
			 || is_mouse_button_event) {

#ifndef QT_NO_QWS_MOUSE_NITPICKER
				mouse->processMouseEvent(ev);
#endif

			} else if (is_key_event && (ev->code() < 128)) {

#ifndef QT_NO_QWS_KEYBOARD_NITPICKER
				keyboard->processKeyEvent(ev);
#endif
			}
		}

	}

//	qDebug() << "QNitpickerInputHandler::readInputData() finished";
}

#endif // !defined(QT_NO_QWS_MOUSE_NITPICKER) || !defined(QT_NO_QWS_KEYBOARD_NITPICKER)
