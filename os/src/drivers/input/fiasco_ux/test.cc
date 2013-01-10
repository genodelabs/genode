/*
 * \brief   Input test
 * \author  Christian Helmuth
 * \date    2006-08-29
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>

namespace Fiasco {
#include <l4/sys/ipc.h>
#include <l4/sys/kernel.h>
#include <l4/sys/vhw.h>

#include <l4/sigma0/sigma0.h>
#include <l4/input/libinput.h>
}

static const char *get_event(unsigned type)
{
	switch (type) {
	case EV_SYN: return "Sync";
	case EV_KEY: return "Key";
	case EV_REL: return "Relative";
	case EV_ABS: return "Absolute";
	case EV_MSC: return "Misc";
	case EV_SW: return "Switch";
	case EV_LED: return "LED";
	case EV_SND: return "Sound";
	case EV_REP: return "Repeat";
	case EV_FF: return "ForceFeedback";
	case EV_PWR: return "Power";
	case EV_FF_STATUS: return "ForceFeedbackStatus";

	default: return 0;
	}
};


static const char *get_key(unsigned code)
{
	switch (code) {
	case KEY_RESERVED: return "Reserved";
	case KEY_ESC: return "Esc";
	case KEY_1: return "1";
	case KEY_2: return "2";
	case KEY_3: return "3";
	case KEY_4: return "4";
	case KEY_5: return "5";
	case KEY_6: return "6";
	case KEY_7: return "7";
	case KEY_8: return "8";
	case KEY_9: return "9";
	case KEY_0: return "0";
	case KEY_MINUS: return "Minus";
	case KEY_EQUAL: return "Equal";
	case KEY_BACKSPACE: return "Backspace";
	case KEY_TAB: return "Tab";
	case KEY_Q: return "Q";
	case KEY_W: return "W";
	case KEY_E: return "E";
	case KEY_R: return "R";
	case KEY_T: return "T";
	case KEY_Y: return "Y";
	case KEY_U: return "U";
	case KEY_I: return "I";
	case KEY_O: return "O";
	case KEY_P: return "P";
	case KEY_LEFTBRACE: return "LeftBrace";
	case KEY_RIGHTBRACE: return "RightBrace";
	case KEY_ENTER: return "Enter";
	case KEY_LEFTCTRL: return "LeftControl";
	case KEY_A: return "A";
	case KEY_S: return "S";
	case KEY_D: return "D";
	case KEY_F: return "F";
	case KEY_G: return "G";
	case KEY_H: return "H";
	case KEY_J: return "J";
	case KEY_K: return "K";
	case KEY_L: return "L";
	case KEY_SEMICOLON: return "Semicolon";
	case KEY_APOSTROPHE: return "Apostrophe";
	case KEY_GRAVE: return "Grave";
	case KEY_LEFTSHIFT: return "LeftShift";
	case KEY_BACKSLASH: return "BackSlash";
	case KEY_Z: return "Z";
	case KEY_X: return "X";
	case KEY_C: return "C";
	case KEY_V: return "V";
	case KEY_B: return "B";
	case KEY_N: return "N";
	case KEY_M: return "M";
	case KEY_COMMA: return "Comma";
	case KEY_DOT: return "Dot";
	case KEY_SLASH: return "Slash";
	case KEY_RIGHTSHIFT: return "RightShift";
	case KEY_KPASTERISK: return "KPAsterisk";
	case KEY_LEFTALT: return "LeftAlt";
	case KEY_SPACE: return "Space";
	case KEY_CAPSLOCK: return "CapsLock";
	case KEY_F1: return "F1";
	case KEY_F2: return "F2";
	case KEY_F3: return "F3";
	case KEY_F4: return "F4";
	case KEY_F5: return "F5";
	case KEY_F6: return "F6";
	case KEY_F7: return "F7";
	case KEY_F8: return "F8";
	case KEY_F9: return "F9";
	case KEY_F10: return "F10";
	case KEY_NUMLOCK: return "NumLock";
	case KEY_SCROLLLOCK: return "ScrollLock";
	case KEY_KP7: return "KP7";
	case KEY_KP8: return "KP8";
	case KEY_KP9: return "KP9";
	case KEY_KPMINUS: return "KPMinus";
	case KEY_KP4: return "KP4";
	case KEY_KP5: return "KP5";
	case KEY_KP6: return "KP6";
	case KEY_KPPLUS: return "KPPlus";
	case KEY_KP1: return "KP1";
	case KEY_KP2: return "KP2";
	case KEY_KP3: return "KP3";
	case KEY_KP0: return "KP0";
	case KEY_KPDOT: return "KPDot";
	case KEY_ZENKAKUHANKAKU: return "Zenkaku/Hankaku";
	case KEY_102ND: return "102nd";
	case KEY_F11: return "F11";
	case KEY_F12: return "F12";
	case KEY_RO: return "RO";
	case KEY_KATAKANA: return "Katakana";
	case KEY_HIRAGANA: return "HIRAGANA";
	case KEY_HENKAN: return "Henkan";
	case KEY_KATAKANAHIRAGANA: return "Katakana/Hiragana";
	case KEY_MUHENKAN: return "Muhenkan";
	case KEY_KPJPCOMMA: return "KPJpComma";
	case KEY_KPENTER: return "KPEnter";
	case KEY_RIGHTCTRL: return "RightCtrl";
	case KEY_KPSLASH: return "KPSlash";
	case KEY_SYSRQ: return "SysRq";
	case KEY_RIGHTALT: return "RightAlt";
	case KEY_LINEFEED: return "LineFeed";
	case KEY_HOME: return "Home";
	case KEY_UP: return "Up";
	case KEY_PAGEUP: return "PageUp";
	case KEY_LEFT: return "Left";
	case KEY_RIGHT: return "Right";
	case KEY_END: return "End";
	case KEY_DOWN: return "Down";
	case KEY_PAGEDOWN: return "PageDown";
	case KEY_INSERT: return "Insert";
	case KEY_DELETE: return "Delete";
	case KEY_MACRO: return "Macro";
	case KEY_MUTE: return "Mute";
	case KEY_VOLUMEDOWN: return "VolumeDown";
	case KEY_VOLUMEUP: return "VolumeUp";
	case KEY_POWER: return "Power";
	case KEY_KPEQUAL: return "KPEqual";
	case KEY_KPPLUSMINUS: return "KPPlusMinus";
	case KEY_PAUSE: return "Pause";
	case KEY_KPCOMMA: return "KPComma";
	case KEY_HANGUEL: return "Hanguel";
	case KEY_HANJA: return "Hanja";
	case KEY_YEN: return "Yen";
	case KEY_LEFTMETA: return "LeftMeta";
	case KEY_RIGHTMETA: return "RightMeta";
	case KEY_COMPOSE: return "Compose";
	case KEY_STOP: return "Stop";
	case KEY_AGAIN: return "Again";
	case KEY_PROPS: return "Props";
	case KEY_UNDO: return "Undo";
	case KEY_FRONT: return "Front";
	case KEY_COPY: return "Copy";
	case KEY_OPEN: return "Open";
	case KEY_PASTE: return "Paste";
	case KEY_FIND: return "Find";
	case KEY_CUT: return "Cut";
	case KEY_HELP: return "Help";
	case KEY_MENU: return "Menu";
	case KEY_CALC: return "Calc";
	case KEY_SETUP: return "Setup";
	case KEY_SLEEP: return "Sleep";
	case KEY_WAKEUP: return "WakeUp";
	case KEY_FILE: return "File";
	case KEY_SENDFILE: return "SendFile";
	case KEY_DELETEFILE: return "DeleteFile";
	case KEY_XFER: return "X-fer";
	case KEY_PROG1: return "Prog1";
	case KEY_PROG2: return "Prog2";
	case KEY_WWW: return "WWW";
	case KEY_MSDOS: return "MSDOS";
	case KEY_COFFEE: return "Coffee";
	case KEY_DIRECTION: return "Direction";
	case KEY_CYCLEWINDOWS: return "CycleWindows";
	case KEY_MAIL: return "Mail";
	case KEY_BOOKMARKS: return "Bookmarks";
	case KEY_COMPUTER: return "Computer";
	case KEY_BACK: return "Back";
	case KEY_FORWARD: return "Forward";
	case KEY_CLOSECD: return "CloseCD";
	case KEY_EJECTCD: return "EjectCD";
	case KEY_EJECTCLOSECD: return "EjectCloseCD";
	case KEY_NEXTSONG: return "NextSong";
	case KEY_PLAYPAUSE: return "PlayPause";
	case KEY_PREVIOUSSONG: return "PreviousSong";
	case KEY_STOPCD: return "StopCD";
	case KEY_RECORD: return "Record";
	case KEY_REWIND: return "Rewind";
	case KEY_PHONE: return "Phone";
	case KEY_ISO: return "ISOKey";
	case KEY_CONFIG: return "Config";
	case KEY_HOMEPAGE: return "HomePage";
	case KEY_REFRESH: return "Refresh";
	case KEY_EXIT: return "Exit";
	case KEY_MOVE: return "Move";
	case KEY_EDIT: return "Edit";
	case KEY_SCROLLUP: return "ScrollUp";
	case KEY_SCROLLDOWN: return "ScrollDown";
	case KEY_KPLEFTPAREN: return "KPLeftParenthesis";
	case KEY_KPRIGHTPAREN: return "KPRightParenthesis";
	case KEY_NEW: return "KEY_NEW";
	case KEY_REDO: return "KEY_REDO";
	case KEY_F13: return "F13";
	case KEY_F14: return "F14";
	case KEY_F15: return "F15";
	case KEY_F16: return "F16";
	case KEY_F17: return "F17";
	case KEY_F18: return "F18";
	case KEY_F19: return "F19";
	case KEY_F20: return "F20";
	case KEY_F21: return "F21";
	case KEY_F22: return "F22";
	case KEY_F23: return "F23";
	case KEY_F24: return "F24";
	case KEY_PLAYCD: return "PlayCD";
	case KEY_PAUSECD: return "PauseCD";
	case KEY_PROG3: return "Prog3";
	case KEY_PROG4: return "Prog4";
	case KEY_SUSPEND: return "Suspend";
	case KEY_CLOSE: return "Close";
	case KEY_PLAY: return "Play";
	case KEY_FASTFORWARD: return "Fast Forward";
	case KEY_BASSBOOST: return "Bass Boost";
	case KEY_PRINT: return "Print";
	case KEY_HP: return "HP";
	case KEY_CAMERA: return "Camera";
	case KEY_SOUND: return "Sound";
	case KEY_QUESTION: return "Question";
	case KEY_EMAIL: return "Email";
	case KEY_CHAT: return "Chat";
	case KEY_SEARCH: return "Search";
	case KEY_CONNECT: return "Connect";
	case KEY_FINANCE: return "Finance";
	case KEY_SPORT: return "Sport";
	case KEY_SHOP: return "Shop";
	case KEY_ALTERASE: return "Alternate Erase";
	case KEY_CANCEL: return "Cancel";
	case KEY_BRIGHTNESSDOWN: return "Brightness down";
	case KEY_BRIGHTNESSUP: return "Brightness up";
	case KEY_MEDIA: return "Media";
	case KEY_SWITCHVIDEOMODE: return "Switch video";
	case KEY_KBDILLUMTOGGLE: return "KBDILLUMTOGGLE";
	case KEY_KBDILLUMDOWN: return "KBDILLUMDOWN";
	case KEY_KBDILLUMUP: return "KBDILLUMUP";
	case KEY_SEND: return "Send";
	case KEY_REPLY: return "Reply";
	case KEY_FORWARDMAIL: return "Forward";
	case KEY_SAVE: return "Save";
	case KEY_DOCUMENTS: return "Documents";
	case KEY_UNKNOWN: return "Unknown";
	case BTN_0: return "Btn0";
	case BTN_1: return "Btn1";
	case BTN_2: return "Btn2";
	case BTN_3: return "Btn3";
	case BTN_4: return "Btn4";
	case BTN_5: return "Btn5";
	case BTN_6: return "Btn6";
	case BTN_7: return "Btn7";
	case BTN_8: return "Btn8";
	case BTN_9: return "Btn9";
	case BTN_LEFT: return "LeftBtn";
	case BTN_RIGHT: return "RightBtn";
	case BTN_MIDDLE: return "MiddleBtn";
	case BTN_SIDE: return "SideBtn";
	case BTN_EXTRA: return "ExtraBtn";
	case BTN_FORWARD: return "ForwardBtn";
	case BTN_BACK: return "BackBtn";
	case BTN_TASK: return "TaskBtn";
	case BTN_TRIGGER: return "Trigger";
	case BTN_THUMB: return "ThumbBtn";
	case BTN_THUMB2: return "ThumbBtn2";
	case BTN_TOP: return "TopBtn";
	case BTN_TOP2: return "TopBtn2";
	case BTN_PINKIE: return "PinkieBtn";
	case BTN_BASE: return "BaseBtn";
	case BTN_BASE2: return "BaseBtn2";
	case BTN_BASE3: return "BaseBtn3";
	case BTN_BASE4: return "BaseBtn4";
	case BTN_BASE5: return "BaseBtn5";
	case BTN_BASE6: return "BaseBtn6";
	case BTN_DEAD: return "BtnDead";
	case BTN_A: return "BtnA";
	case BTN_B: return "BtnB";
	case BTN_C: return "BtnC";
	case BTN_X: return "BtnX";
	case BTN_Y: return "BtnY";
	case BTN_Z: return "BtnZ";
	case BTN_TL: return "BtnTL";
	case BTN_TR: return "BtnTR";
	case BTN_TL2: return "BtnTL2";
	case BTN_TR2: return "BtnTR2";
	case BTN_SELECT: return "BtnSelect";
	case BTN_START: return "BtnStart";
	case BTN_MODE: return "BtnMode";
	case BTN_THUMBL: return "BtnThumbL";
	case BTN_THUMBR: return "BtnThumbR";
	case BTN_TOOL_PEN: return "ToolPen";
	case BTN_TOOL_RUBBER: return "ToolRubber";
	case BTN_TOOL_BRUSH: return "ToolBrush";
	case BTN_TOOL_PENCIL: return "ToolPencil";
	case BTN_TOOL_AIRBRUSH: return "ToolAirbrush";
	case BTN_TOOL_FINGER: return "ToolFinger";
	case BTN_TOOL_MOUSE: return "ToolMouse";
	case BTN_TOOL_LENS: return "ToolLens";
	case BTN_TOUCH: return "Touch";
	case BTN_STYLUS: return "Stylus";
	case BTN_STYLUS2: return "Stylus2";
	case BTN_TOOL_DOUBLETAP: return "Tool Doubletap";
	case BTN_TOOL_TRIPLETAP: return "Tool Tripletap";
	case BTN_GEAR_DOWN: return "WheelBtn";
	case BTN_GEAR_UP: return "Gear up";
	case KEY_OK: return "Ok";
	case KEY_SELECT: return "Select";
	case KEY_GOTO: return "Goto";
	case KEY_CLEAR: return "Clear";
	case KEY_POWER2: return "Power2";
	case KEY_OPTION: return "Option";
	case KEY_INFO: return "Info";
	case KEY_TIME: return "Time";
	case KEY_VENDOR: return "Vendor";
	case KEY_ARCHIVE: return "Archive";
	case KEY_PROGRAM: return "Program";
	case KEY_CHANNEL: return "Channel";
	case KEY_FAVORITES: return "Favorites";
	case KEY_EPG: return "EPG";
	case KEY_PVR: return "PVR";
	case KEY_MHP: return "MHP";
	case KEY_LANGUAGE: return "Language";
	case KEY_TITLE: return "Title";
	case KEY_SUBTITLE: return "Subtitle";
	case KEY_ANGLE: return "Angle";
	case KEY_ZOOM: return "Zoom";
	case KEY_MODE: return "Mode";
	case KEY_KEYBOARD: return "Keyboard";
	case KEY_SCREEN: return "Screen";
	case KEY_PC: return "PC";
	case KEY_TV: return "TV";
	case KEY_TV2: return "TV2";
	case KEY_VCR: return "VCR";
	case KEY_VCR2: return "VCR2";
	case KEY_SAT: return "Sat";
	case KEY_SAT2: return "Sat2";
	case KEY_CD: return "CD";
	case KEY_TAPE: return "Tape";
	case KEY_RADIO: return "Radio";
	case KEY_TUNER: return "Tuner";
	case KEY_PLAYER: return "Player";
	case KEY_TEXT: return "Text";
	case KEY_DVD: return "DVD";
	case KEY_AUX: return "Aux";
	case KEY_MP3: return "MP3";
	case KEY_AUDIO: return "Audio";
	case KEY_VIDEO: return "Video";
	case KEY_DIRECTORY: return "Directory";
	case KEY_LIST: return "List";
	case KEY_MEMO: return "Memo";
	case KEY_CALENDAR: return "Calendar";
	case KEY_RED: return "Red";
	case KEY_GREEN: return "Green";
	case KEY_YELLOW: return "Yellow";
	case KEY_BLUE: return "Blue";
	case KEY_CHANNELUP: return "ChannelUp";
	case KEY_CHANNELDOWN: return "ChannelDown";
	case KEY_FIRST: return "First";
	case KEY_LAST: return "Last";
	case KEY_AB: return "AB";
	case KEY_NEXT: return "Next";
	case KEY_RESTART: return "Restart";
	case KEY_SLOW: return "Slow";
	case KEY_SHUFFLE: return "Shuffle";
	case KEY_BREAK: return "Break";
	case KEY_PREVIOUS: return "Previous";
	case KEY_DIGITS: return "Digits";
	case KEY_TEEN: return "TEEN";
	case KEY_TWEN: return "TWEN";
	case KEY_DEL_EOL: return "Delete EOL";
	case KEY_DEL_EOS: return "Delete EOS";
	case KEY_INS_LINE: return "Insert line";
	case KEY_DEL_LINE: return "Delete line";
	case KEY_FN: return "KEY_FN";
	case KEY_FN_ESC: return "KEY_FN_ESC";
	case KEY_FN_F1: return "KEY_FN_F1";
	case KEY_FN_F2: return "KEY_FN_F2";
	case KEY_FN_F3: return "KEY_FN_F3";
	case KEY_FN_F4: return "KEY_FN_F4";
	case KEY_FN_F5: return "KEY_FN_F5";
	case KEY_FN_F6: return "KEY_FN_F6";
	case KEY_FN_F7: return "KEY_FN_F7";
	case KEY_FN_F8: return "KEY_FN_F8";
	case KEY_FN_F9: return "KEY_FN_F9";
	case KEY_FN_F10: return "KEY_FN_F10";
	case KEY_FN_F11: return "KEY_FN_F11";
	case KEY_FN_F12: return "KEY_FN_F12";
	case KEY_FN_1: return "KEY_FN_1";
	case KEY_FN_2: return "KEY_FN_2";
	case KEY_FN_D: return "KEY_FN_D";
	case KEY_FN_E: return "KEY_FN_E";
	case KEY_FN_F: return "KEY_FN_F";
	case KEY_FN_S: return "KEY_FN_S";
	case KEY_FN_B: return "KEY_FN_B";

	default: return 0;
	}
};


static const char *get_rel(unsigned code)
{
	switch (code) {
	case REL_X: return "X";
	case REL_Y: return "Y";
	case REL_Z: return "Z";
	case REL_RX: return "RX";
	case REL_RY: return "RY";
	case REL_RZ: return "RZ";
	case REL_HWHEEL: return "HWheel";
	case REL_DIAL: return "Dial";
	case REL_WHEEL: return "Wheel";
	case REL_MISC: return "Misc";

	default: return 0;
	}
};


static const char *get_abs(unsigned code)
{
	switch (code) {
	case ABS_X: return "X";
	case ABS_Y: return "Y";
	case ABS_Z: return "Z";
	case ABS_RX: return "Rx";
	case ABS_RY: return "Ry";
	case ABS_RZ: return "Rz";
	case ABS_THROTTLE: return "Throttle";
	case ABS_RUDDER: return "Rudder";
	case ABS_WHEEL: return "Wheel";
	case ABS_GAS: return "Gas";
	case ABS_BRAKE: return "Brake";
	case ABS_HAT0X: return "Hat0X";
	case ABS_HAT0Y: return "Hat0Y";
	case ABS_HAT1X: return "Hat1X";
	case ABS_HAT1Y: return "Hat1Y";
	case ABS_HAT2X: return "Hat2X";
	case ABS_HAT2Y: return "Hat2Y";
	case ABS_HAT3X: return "Hat3X";
	case ABS_HAT3Y: return "Hat 3Y";
	case ABS_PRESSURE: return "Pressure";
	case ABS_DISTANCE: return "Distance";
	case ABS_TILT_X: return "XTilt";
	case ABS_TILT_Y: return "YTilt";
	case ABS_TOOL_WIDTH: return "Tool Width";
	case ABS_VOLUME: return "Volume";
	case ABS_MISC: return "Misc";

	default: return 0;
	}
};


static const char *get_misc(unsigned code)
{
	switch (code) {
	case MSC_SERIAL: return "Serial";
	case MSC_PULSELED: return "Pulseled";
	case MSC_GESTURE: return "Gesture";
	case MSC_RAW: return "RawData";
	case MSC_SCAN: return "ScanCode";

	default: return 0;
	}
};


//char *leds[LED_MAX + 1: return {
//	case 0 ... LED_MAX: return NULL;
//	case LED_NUML: return "NumLock";
//	case LED_CAPSL: return "CapsLock", 
//	case LED_SCROLLL: return "ScrollLock";
//	case LED_COMPOSE: return "Compose";
//	case LED_KANA: return "Kana";
//	case LED_SLEEP: return "Sleep", 
//	case LED_SUSPEND: return "Suspend";
//	case LED_MUTE: return "Mute";
//	case LED_MISC: return "Misc";
//	case LED_MAIL: return "Mail";
//	case LED_CHARGING: return "Charging";
//};
//
//char *repeats[REP_MAX + 1: return {
//	case 0 ... REP_MAX: return NULL;
//	case REP_DELAY: return "Delay";
//	case REP_PERIOD: return "Period";
//};
//
//char *sounds[SND_MAX + 1: return {
//	case 0 ... SND_MAX: return NULL;
//	case SND_CLICK: return "Click";
//	case SND_BELL: return "Bell";
//	case SND_TONE: return "Tone";
//};


static const char *get_name(unsigned type, unsigned code)
{
	switch (type)
	{
	case EV_SYN: return get_event(code);
	case EV_KEY: return get_key(code);
	case EV_REL: return get_rel(code);
	case EV_ABS: return get_abs(code);
	case EV_MSC: return get_misc(code);
//	case EV_LED: return leds;
//	case EV_SND: return sounds
//	case EV_REP: return repeats;

	default: return 0;
	}
};

namespace Fiasco {

	void log_event(struct l4input *ev)
	{
		if (!ev->type)
			PDBG("type = 0\n");

		PDBG("Event: type %d (%s), code %d (%s), value %d",
		     ev->type, get_event(ev->type),
		     ev->code, get_name(ev->type, ev->code),
		     ev->value);
	}
}
