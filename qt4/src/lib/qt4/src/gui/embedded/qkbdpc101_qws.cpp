/****************************************************************************
**
** Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
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
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qkbdpc101_qws.h"

#ifndef QT_NO_QWS_KEYBOARD

#include "qscreen_qws.h"
#include "qwindowsystem_qws.h"
#include "qnamespace.h"
#include "qapplication.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifdef Q_OS_LINUX
#include <sys/kd.h>
#include <sys/vt.h>
#endif

QT_BEGIN_NAMESPACE

static const QWSKeyMap pc101KeyM[] = {
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Escape,     27      , 27      , 0xffff  },
    {   Qt::Key_1,      '1'     , '!'     , 0xffff  },
    {   Qt::Key_2,      '2'     , '@'     , 0xffff  },
    {   Qt::Key_3,      '3'     , '#'     , 0xffff  },
    {   Qt::Key_4,      '4'     , '$'     , 0xffff  },
    {   Qt::Key_5,      '5'     , '%'     , 0xffff  },
    {   Qt::Key_6,      '6'     , '^'     , 0xffff  },
    {   Qt::Key_7,      '7'     , '&'     , 0xffff  },
    {   Qt::Key_8,      '8'     , '*'     , 0xffff  },
    {   Qt::Key_9,      '9'     , '('     , 0xffff  },  // 10
    {   Qt::Key_0,      '0'     , ')'     , 0xffff  },
    {   Qt::Key_Minus,      '-'     , '_'     , 0xffff  },
    {   Qt::Key_Equal,      '='     , '+'     , 0xffff  },
    {   Qt::Key_Backspace,  8       , 8       , 0xffff  },
    {   Qt::Key_Tab,        9       , 9       , 0xffff  },
    {   Qt::Key_Q,      'q'     , 'Q'     , 'Q'-64  },
    {   Qt::Key_W,      'w'     , 'W'     , 'W'-64  },
    {   Qt::Key_E,      'e'     , 'E'     , 'E'-64  },
    {   Qt::Key_R,      'r'     , 'R'     , 'R'-64  },
    {   Qt::Key_T,      't'     , 'T'     , 'T'-64  },  // 20
    {   Qt::Key_Y,      'y'     , 'Y'     , 'Y'-64  },
    {   Qt::Key_U,      'u'     , 'U'     , 'U'-64  },
    {   Qt::Key_I,      'i'     , 'I'     , 'I'-64  },
    {   Qt::Key_O,      'o'     , 'O'     , 'O'-64  },
    {   Qt::Key_P,      'p'     , 'P'     , 'P'-64  },
    {   Qt::Key_BraceLeft,  '['     , '{'     , 0xffff  },
    {   Qt::Key_BraceRight, ']'     , '}'     , 0xffff  },
    {   Qt::Key_Return,     13      , 13      , 0xffff  },
    {   Qt::Key_Control,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_A,      'a'     , 'A'     , 'A'-64  },  // 30
    {   Qt::Key_S,      's'     , 'S'     , 'S'-64  },
    {   Qt::Key_D,      'd'     , 'D'     , 'D'-64  },
    {   Qt::Key_F,      'f'     , 'F'     , 'F'-64  },
    {   Qt::Key_G,      'g'     , 'G'     , 'G'-64  },
    {   Qt::Key_H,      'h'     , 'H'     , 'H'-64  },
    {   Qt::Key_J,      'j'     , 'J'     , 'J'-64  },
    {   Qt::Key_K,      'k'     , 'K'     , 'K'-64  },
    {   Qt::Key_L,      'l'     , 'L'     , 'L'-64  },
    {   Qt::Key_Semicolon,  ';'     , ':'     , 0xffff  },
    {   Qt::Key_Apostrophe, '\''    , '"'     , 0xffff  },  // 40
    {   Qt::Key_QuoteLeft,  '`'     , '~'     , 0xffff  },
    {   Qt::Key_Shift,      0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Backslash,  '\\'    , '|'     , 0xffff  },
    {   Qt::Key_Z,      'z'     , 'Z'     , 'Z'-64  },
    {   Qt::Key_X,      'x'     , 'X'     , 'X'-64  },
    {   Qt::Key_C,      'c'     , 'C'     , 'C'-64  },
    {   Qt::Key_V,      'v'     , 'V'     , 'V'-64  },
    {   Qt::Key_B,      'b'     , 'B'     , 'B'-64  },
    {   Qt::Key_N,      'n'     , 'N'     , 'N'-64  },
    {   Qt::Key_M,      'm'     , 'M'     , 'M'-64  },  // 50
    {   Qt::Key_Comma,      ','     , '<'     , 0xffff  },
    {   Qt::Key_Period,     '.'     , '>'     , 0xffff  },
    {   Qt::Key_Slash,      '/'     , '?'     , 0xffff  },
    {   Qt::Key_Shift,      0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Asterisk,   '*'     , '*'     , 0xffff  },
    {   Qt::Key_Alt,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Space,      ' '     , ' '     , 0xffff  },
    {   Qt::Key_CapsLock,   0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F1,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F2,     0xffff  , 0xffff  , 0xffff  },  // 60
    {   Qt::Key_F3,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F4,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F5,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F6,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F7,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F8,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F9,     0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F10,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_NumLock,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_ScrollLock, 0xffff  , 0xffff  , 0xffff  },  // 70
    {   Qt::Key_7,      '7'     , '7'     , 0xffff  },
    {   Qt::Key_8,      '8'     , '8'     , 0xffff  },
    {   Qt::Key_9,      '9'     , '9'     , 0xffff  },
    {   Qt::Key_Minus,      '-'     , '-'     , 0xffff  },
    {   Qt::Key_4,      '4'     , '4'     , 0xffff  },
    {   Qt::Key_5,      '5'     , '5'     , 0xffff  },
    {   Qt::Key_6,      '6'     , '6'     , 0xffff  },
    {   Qt::Key_Plus,       '+'     , '+'     , 0xffff  },
    {   Qt::Key_1,      '1'     , '1'     , 0xffff  },
    {   Qt::Key_2,      '2'     , '2'     , 0xffff  },  // 80
    {   Qt::Key_3,      '3'     , '3'     , 0xffff  },
    {   Qt::Key_0,      '0'     , '0'     , 0xffff  },
    {   Qt::Key_Period,     '.'     , '.'     , 0xffff  },
    {   Qt::Key_SysReq,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Less,   '<'     , '>'  , 0xffff  },
    {   Qt::Key_F11,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F12,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  }, // 90
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_unknown,    0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Enter,      13      , 13      , 0xffff  },
    {   Qt::Key_Control,    0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Slash,                '/'     , '/'     , 0xffff  },
    {   Qt::Key_SysReq,    0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Meta,                0xffff  , 0xffff  , 0xffff  }, // 100
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  }, // break
    {        Qt::Key_Home,            0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Up,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_PageUp,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Left,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Right,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_End,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Down,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_PageDown,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_Insert,                0xffff  , 0xffff  , 0xffff  }, // 110
    {        Qt::Key_Delete,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  }, // macro
    {   Qt::Key_F13,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_F14,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Help,       0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  }, // do
    {   Qt::Key_F17,        0xffff  , 0xffff  , 0xffff  },
    {   Qt::Key_Plus,       '+'     , '-'     , 0xffff  },
    {        Qt::Key_Pause,                0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {        Qt::Key_unknown,        0xffff  , 0xffff  , 0xffff  },
    {   0,          0xffff  , 0xffff  , 0xffff  }
};

static const int keyMSize = sizeof(pc101KeyM)/sizeof(QWSKeyMap)-1;

//===========================================================================

//
// PC-101 type keyboards
//

/*!
    \class QWSPC101KeyboardHandler
    \ingroup qws

    \internal
*/

QWSPC101KeyboardHandler::QWSPC101KeyboardHandler(const QString&)
{
    shift = false;
    alt   = false;
    ctrl  = false;
    extended = 0;
    prevuni = 0;
    prevkey = 0;
    caps = false;
#if defined(QT_QWS_IPAQ)
    // iPAQ Action Key has ScanCode 0x60: 0x60|0x80 = 0xe0 == extended mode 1 !
    ipaq_return_pressed = false;
#endif
}

QWSPC101KeyboardHandler::~QWSPC101KeyboardHandler()
{
}

const QWSKeyMap *QWSPC101KeyboardHandler::keyMap() const
{
    return pc101KeyM;
}

void QWSPC101KeyboardHandler::doKey(uchar code)
{
    int keyCode = Qt::Key_unknown;
    bool release = false;
    int keypad = 0;
#ifdef Q_OS_GENODE
    bool softwareRepeat = true;
#else
    bool softwareRepeat = false;
#endif

#ifndef QT_QWS_USE_KEYCODES
    // extended?
    if (code == 224
#if defined(QT_QWS_IPAQ)
        && !ipaq_return_pressed
#endif
   ) {
        extended = 1;
        return;
    } else if (code == 225) {
        extended = 2;
        return;
    }
#endif

    if (code & 0x80) {
        release = true;
        code &= 0x7f;
    }

#ifndef QT_QWS_USE_KEYCODES
    if (extended == 1) {
        switch (code) {
        case 72:
            keyCode = Qt::Key_Up;
            break;
        case 75:
            keyCode = Qt::Key_Left;
            break;
        case 77:
            keyCode = Qt::Key_Right;
            break;
        case 80:
            keyCode = Qt::Key_Down;
            break;
        case 82:
            keyCode = Qt::Key_Insert;
            break;
        case 71:
            keyCode = Qt::Key_Home;
            break;
        case 73:
            keyCode = Qt::Key_PageUp;
            break;
        case 83:
            keyCode = Qt::Key_Delete;
            break;
        case 79:
            keyCode = Qt::Key_End;
            break;
        case 81:
            keyCode = Qt::Key_PageDown;
            break;
        case 28:
            keyCode = Qt::Key_Enter;
            break;
        case 53:
            keyCode = Qt::Key_Slash;
            break;
        case 0x1d:
            keyCode = Qt::Key_Control;
            break;
        case 0x2a:
            keyCode = Qt::Key_Print;
            break;
        case 0x38:
            keyCode = Qt::Key_Alt;
            break;
        case 0x5b:
            keyCode = Qt::Key_Super_L;
            break;
        case 0x5c:
            keyCode = Qt::Key_Super_R;
            break;
        case 0x5d:
            keyCode = Qt::Key_Menu;
            break;
#if 0
        default:
            qDebug("extended1 code %x release %d", code, release);
            break;
#endif
        }
    } else if (extended == 2) {
        switch (code) {
        case 0x1d:
            return;
        case 0x45:
            keyCode = Qt::Key_Pause;
            break;
        }
    } else
#endif
    {
        if (code < keyMSize) {
            keyCode = pc101KeyM[code].key_code;
        }

#if defined(QT_QWS_IPAQ) || defined(QT_QWS_EBX)
        softwareRepeat = true;

        switch (code) {
            case 0x7a: case 0x7b: case 0x7c: case 0x7d:
                keyCode = code - 0x7a + Qt::Key_F9;
                softwareRepeat = false;
                break;
            case 0x79:
                keyCode = Qt::Key_SysReq;
                softwareRepeat = false;
                break;
            case 0x78:
# ifdef QT_QWS_IPAQ
                keyCode = Qt::Key_F24;  // record
# else
                keyCode = Qt::Key_Escape;
# endif
                softwareRepeat = false;
                break;
            case 0x60:
                keyCode = Qt::Key_Return;
# ifdef QT_QWS_IPAQ
                ipaq_return_pressed = !release;
# endif
                break;
            case 0x67:
                keyCode = Qt::Key_Right;
                break;
            case 0x69:
                keyCode = Qt::Key_Up;
                break;
            case 0x6a:
                keyCode = Qt::Key_Down;
                break;
            case 0x6c:
                keyCode = Qt::Key_Left;
                break;
        }

        if (qt_screen->isTransformed()
                && keyCode >= Qt::Key_Left && keyCode <= Qt::Key_Down)
        {
            keyCode = transformDirKey(keyCode);
        }
#endif
        /*
          Translate shift+Qt::Key_Tab to Qt::Key_Backtab
        */
        if ((keyCode == Qt::Key_Tab) && shift)
            keyCode = Qt::Key_Backtab;
    }

#ifndef QT_QWS_USE_KEYCODES
    /*
      Qt::Keypad consists of extended keys 53 and 28,
      and non-extended keys 55 and 71 through 83.
    */
    if ((extended == 1) ? (code == 53 || code == 28) :
         (code == 55 || (code >= 71 && code <= 83)))
#else
    if (code == 55 || code >= 71 && code <= 83 || code == 96
            || code == 98 || code == 118)
#endif
    {
        keypad = Qt::KeypadModifier;
    }

    // Ctrl-Alt-Backspace exits qws
    if (ctrl && alt && keyCode == Qt::Key_Backspace) {
        qApp->quit();
    }

    if (keyCode == Qt::Key_Alt) {
        alt = !release;
    } else if (keyCode == Qt::Key_Control) {
        ctrl = !release;
    } else if (keyCode == Qt::Key_Shift) {
        shift = !release;
    } else if (keyCode == Qt::Key_CapsLock && release) {
        caps = !caps;
#if defined(Q_OS_LINUX)
        char leds;
        ioctl(0, KDGETLED, &leds);
        leds = leds & ~LED_CAP;
        if (caps) leds |= LED_CAP;
        ioctl(0, KDSETLED, leds);
#endif
    }
    if (keyCode != Qt::Key_unknown) {
        bool bAlt = alt;
        bool bCtrl = ctrl;
        bool bShift = shift;
        int unicode = 0;
        if (code < keyMSize) {
            if (!extended) {
                bool bCaps = shift ||
                    (caps ? QChar(keyMap()[code].unicode).isLetter() : false);
                if (bCtrl)
                    unicode =  keyMap()[code].ctrl_unicode ?  keyMap()[code].ctrl_unicode : 0xffff;
                else if (bCaps)
                    unicode =  keyMap()[code].shift_unicode ?  keyMap()[code].shift_unicode : 0xffff;
                else
                    unicode =  keyMap()[code].unicode ?  keyMap()[code].unicode : 0xffff;
#ifndef QT_QWS_USE_KEYCODES
            } else if (extended==1) {
                if (code == 53)
                    unicode = '/';
#endif
            }
        }

        modifiers = 0;
        if (bAlt) modifiers |= Qt::AltModifier;
        if (bCtrl) modifiers |= Qt::ControlModifier;
        if (bShift) modifiers |= Qt::ShiftModifier;
        if (keypad) modifiers |= Qt::KeypadModifier;

        if (softwareRepeat && release)
          endAutoRepeat();

        // looks wrong -- WWA
        bool repeat = false;
        if (prevuni == unicode && prevkey == keyCode && !release)
            repeat = true;

        processKeyEvent(unicode, keyCode, modifiers, !release, repeat);

        if (!release) {
            prevuni = unicode;
            prevkey = keyCode;
        } else {
            prevkey = prevuni = 0;
        }
    }

    if (softwareRepeat && !release) {
        // process all pending events before starting the autorepeat timer to prevent unwanted repetitions
        QApplication::processEvents();

        beginAutoRepeat(prevuni, prevkey, modifiers);
    }

    extended = 0;
}

QT_END_NAMESPACE

#endif // QT_NO_QWS_KEYBOARD
