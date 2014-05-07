#ifndef QINPUTNITPICKER_QWS_H
#define QINPUTNITPICKER_QWS_H

#include "qmousenitpicker_qws.h"
#include "qkbdnitpicker_qws.h"

QT_BEGIN_HEADER

QT_MODULE(Gui)

#if !defined(QT_NO_QWS_MOUSE_NITPICKER) || !defined(QT_NO_QWS_KEYBOARD_NITPICKER)

#include <base/capability.h>
#include <input/event.h>
#include <input_session/client.h>

class QNitpickerInputHandler : public QObject {

    Q_OBJECT

public:
    QNitpickerInputHandler(QScreen *screen,
                           Input::Session_capability input_session_cap,
                           const QString &driver = QString(),
                           const QString &device = QString());
    ~QNitpickerInputHandler();

    void setScreen(const QScreen *screen);

private:
    Input::Session_client *input;
    Input::Event *ev_buf;

#ifndef QT_NO_QWS_MOUSE_NITPICKER
    QNitpickerMouseHandler *mouse;
#endif

#ifndef QT_NO_QWS_KEYBOARD_NITPICKER
    QNitpickerKeyboardHandler *keyboard;
#endif

    QTimer *timer;

private Q_SLOTS:
    void readInputData();
};

#endif // !defined(QT_NO_QWS_MOUSE_NITPICKER) || !defined(QT_NO_QWS_KEYBOARD_NITPICKER)

QT_END_HEADER

#endif // QINPUTNITPICKER_QWS_H
