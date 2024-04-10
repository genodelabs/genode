/*
 * \brief  Libxkbcommon-based keyboard-layout generator
 * \author Christian Helmuth
 * \date   2019-07-16
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Linux includes */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xkbcommon/xkbcommon-compose.h>
#include <set>
#include <vector>
#include <stdexcept>

/* Genode includes */
#include <util/xml_generator.h>
#include <util/retry.h>
#include <util/reconstructible.h>

#include "xkb_mapping.h"
#include "util.h"

using Genode::Xml_generator;
using Genode::Constructible;


static void append_comment(Xml_generator &xml, char const *prefix,
                           char const *comment, char const *suffix)
{
	xml.append(prefix);
	xml.append("<!-- "); xml.append(comment); xml.append(" -->");
	xml.append(suffix);
}


/*
 * XML generator that expands to your needs
 */
class Expanding_xml_buffer
{
	private:

		char const *_name;

		enum { BUFFER_INCREMENT = 1024*1024 };

		size_t  _buffer_size { 0 };
		char   *_buffer      { nullptr };

		void _increase_buffer()
		{
			::free(_buffer);

			_buffer_size += BUFFER_INCREMENT;
			_buffer       = (char *)::calloc(1, _buffer_size);
		}

	public:

		Expanding_xml_buffer() { _increase_buffer(); }

		~Expanding_xml_buffer() { ::free(_buffer); }

		char const *buffer() const { return _buffer; }

		template <typename FUNC>
		void generate(char const *name, FUNC const &func)
		{
			Genode::retry<Xml_generator::Buffer_exceeded>(
				[&] () {
					Xml_generator xml(_buffer, _buffer_size,
					                  name, [&] () { func(xml); });
				},
				[&] () { _increase_buffer(); }
			);
		}
};


static bool keysym_composing(xkb_compose_state *compose_state, xkb_keysym_t sym)
{
	xkb_compose_state_reset(compose_state);
	xkb_compose_state_feed(compose_state, sym);

	return (XKB_COMPOSE_COMPOSING == xkb_compose_state_get_status(compose_state));
}


struct Key_info
{
	enum { COMMENT_CAPACITY = 100 };

	xkb_keysym_t  _keysym    { XKB_KEY_NoSymbol };
	bool          _composing { false };
	unsigned      _utf32     { 0 };
	char         *_comment   { (char *)::calloc(1, COMMENT_CAPACITY) };

	Key_info(xkb_state *state, xkb_compose_state *compose_state, Input::Keycode code)
	{
		_keysym = xkb_state_key_get_one_sym(state, Xkb::keycode(code));
		if (_keysym == XKB_KEY_NoSymbol) return;

		_composing = keysym_composing(compose_state, _keysym);

		if (!_composing) {
			_utf32 = xkb_state_key_get_utf32(state, Xkb::keycode(code));
			xkb_state_key_get_utf8(state, Xkb::keycode(code), _comment, COMMENT_CAPACITY);
		} else {
			char keysym_str[32];
			xkb_keysym_get_name(_keysym, keysym_str, sizeof(keysym_str));

			for (Xkb::Dead_keysym &d : Xkb::dead_keysym) {
				if (d.xkb != _keysym) continue;

				_utf32 = d.utf32;
				strncat(_comment, keysym_str, COMMENT_CAPACITY - 1);
				return;
			}
			::fprintf(stderr, "no UTF32 mapping found for composing keysym <%s>\n", keysym_str);
		}
	}

	~Key_info()
	{
		::free(_comment);
	}

	bool valid()          const { return _utf32  != 0; }
	xkb_keysym_t keysym() const { return _keysym; }
	bool composing()      const { return _composing; }
	unsigned utf32()      const { return _utf32; }

	void attributes(Xml_generator &xml)
	{
		xml.attribute("code", Formatted("0x%04x", _utf32).string());
	}

	void comment(Xml_generator &xml)
	{
		append_comment(xml, "\t", _comment, "");
	}
};


struct Keysym
{
	bool         composing { 0 };
	xkb_keysym_t keysym    { 0 };
	unsigned     utf32     { 0 };
};


static bool operator < (Keysym const &a, Keysym const &b)
{
	return a.keysym < b.keysym;
}


template <Input::Keycode code>
struct Locked
{
	xkb_state *state;

	Locked(xkb_state *state) : state(state)
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_DOWN);
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_UP);
	}

	~Locked()
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_DOWN);
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_UP);
	}
};


template <Input::Keycode code>
struct Pressed
{
	xkb_state *state;

	Pressed(xkb_state *state) : state(state)
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_DOWN);
	}

	~Pressed()
	{
		xkb_state_update_key(state, Xkb::keycode(code), XKB_KEY_UP);
	}
};


struct Args
{
	struct Invalid_args { };

	enum class Command { GENERATE, DUMP, INFO };

	Command     command;
	char const *layout;
	char const *variant;
	char const *locale;

	char const *usage =
		"usage: xkb2ifcfg <command> <layout> <variant> <locale>\n"
		"\n"
		"  Commands\n"
		"\n"
		"    generate   generate input_filter config\n"
		"    dump       dump raw XKB keymap\n"
		"    info       simple per-key information\n"
		"\n"
		"  Example\n"
		"\n"
		"    xkb2ifcfg generate us ''         en_US.UTF-8\n"
		"    xkb2ifcfg info     de nodeadkeys de_DE.UTF-8\n";

	Args(int argc, char **argv)
	try {
		if (argc != 5) throw Invalid_args();

		if      (!::strcmp("generate", argv[1])) command = Command::GENERATE;
		else if (!::strcmp("dump",     argv[1])) command = Command::DUMP;
		else if (!::strcmp("info",     argv[1])) command = Command::INFO;
		else throw Invalid_args();

		layout  = argv[2];
		variant = argv[3];
		locale  = argv[4];

		if (!strlen(layout) || !strlen(locale))
			throw Invalid_args();
	} catch (...) { ::fputs(usage, stderr); throw; }
};


class Main
{
	public:

		/* initialization-failure exceptions */
		struct Xkb_init_failed { };
		struct Keymap_init_failed { };
		struct Compose_init_failed { };

	private:

		struct Map;
		struct Sequence;

		Args args;

		xkb_context       *_context;
		xkb_rule_names     _rmlvo;
		xkb_keymap        *_keymap;
		xkb_state         *_state;
		xkb_compose_table *_compose_table;
		xkb_compose_state *_compose_state;

		std::set<Keysym> _keysyms;

		/*
		 * Numpad keys are remapped in input_filter if numlock=off, so we
		 * always assume numlock=on to handle KP1 etc. correctly.
		 */
		Constructible<Locked<Input::KEY_NUMLOCK>> _numlock;

		/* utilities */
		char const * _string(enum xkb_compose_status);
		char const * _string(enum xkb_compose_feed_result);

		void _keycode_info(xkb_keycode_t);
		void _keycode_xml_non_printable(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_control(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_altgr(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_capslock(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift_altgr(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift_capslock(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_altgr_capslock(Xml_generator &, xkb_keycode_t);
		void _keycode_xml_printable_shift_altgr_capslock(Xml_generator &, xkb_keycode_t);

		int _generate();
		int _dump();
		int _info();

	public:

		Main(int argc, char **argv);
		~Main();

		xkb_keymap * keymap() { return _keymap; }

		int exec();
};


struct Main::Map
{
	Main          &main;
	Xml_generator &xml;

	enum class Mod : unsigned {
		NONE                 = 0,
		SHIFT                = 0b0001, /* mod1 */
		CONTROL              = 0b0010, /* mod2 */
		ALTGR                = 0b0100, /* mod3 */
		CAPSLOCK             = 0b1000, /* mod4 */
		SHIFT_ALTGR          = SHIFT | ALTGR,
		SHIFT_CAPSLOCK       = SHIFT | CAPSLOCK,
		ALTGR_CAPSLOCK       = ALTGR | CAPSLOCK,
		SHIFT_ALTGR_CAPSLOCK = SHIFT | ALTGR | CAPSLOCK,
	} mod;

	static char const * _string(Mod mod)
	{
		switch (mod) {
		case Map::Mod::NONE:                 return "no modifier";
		case Map::Mod::SHIFT:                return "SHIFT";
		case Map::Mod::CONTROL:              return "CONTROL";
		case Map::Mod::ALTGR:                return "ALTGR";
		case Map::Mod::CAPSLOCK:             return "CAPSLOCK";
		case Map::Mod::SHIFT_ALTGR:          return "SHIFT-ALTGR";
		case Map::Mod::SHIFT_CAPSLOCK:       return "SHIFT-CAPSLOCK";
		case Map::Mod::ALTGR_CAPSLOCK:       return "ALTGR-CAPSLOCK";
		case Map::Mod::SHIFT_ALTGR_CAPSLOCK: return "SHIFT-ALTGR-CAPSLOCK";
		}
		return "invalid";
	}

	static void _non_printable(xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		Map &m = *reinterpret_cast<Map *>(data);

		if (m.mod != Map::Mod::NONE) {
			::fprintf(stderr, "%s: mod=%u not supported\n",
			          __PRETTY_FUNCTION__, unsigned(m.mod));
			return;
		}

		m.main._keycode_xml_non_printable(m.xml, keycode);
	}

	static void _control(xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		Map &m = *reinterpret_cast<Map *>(data);

		if (m.mod != Map::Mod::CONTROL) {
			::fprintf(stderr, "%s: mod=%u not supported\n",
			          __PRETTY_FUNCTION__, unsigned(m.mod));
			return;
		}

		m.main._keycode_xml_control(m.xml, keycode);
	}

	static void _printable(xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		Map &m = *reinterpret_cast<Map *>(data);

		switch (m.mod) {
		case Map::Mod::NONE:
			m.main._keycode_xml_printable(m.xml, keycode); break;
		case Map::Mod::SHIFT:
			m.main._keycode_xml_printable_shift(m.xml, keycode); break;
		case Map::Mod::CONTROL:
			/* not printable */ break;
		case Map::Mod::ALTGR:
			m.main._keycode_xml_printable_altgr(m.xml, keycode); break;
		case Map::Mod::CAPSLOCK:
			m.main._keycode_xml_printable_capslock(m.xml, keycode); break;
		case Map::Mod::SHIFT_ALTGR:
			m.main._keycode_xml_printable_shift_altgr(m.xml, keycode); break;
		case Map::Mod::SHIFT_CAPSLOCK:
			m.main._keycode_xml_printable_shift_capslock(m.xml, keycode); break;
		case Map::Mod::ALTGR_CAPSLOCK:
			m.main._keycode_xml_printable_altgr_capslock(m.xml, keycode); break;
		case Map::Mod::SHIFT_ALTGR_CAPSLOCK:
			m.main._keycode_xml_printable_shift_altgr_capslock(m.xml, keycode); break;
		}
	}

	Map(Main &main, Xml_generator &xml, Mod mod)
	:
		main(main), xml(xml), mod(mod)
	{
		if (mod == Mod::NONE) {
			/* generate basic character map */
			xml.node("map", [&] ()
			{
				append_comment(xml, "\n\t\t", "printable", "");
				xkb_keymap_key_for_each(main.keymap(), _printable, this);

				append_comment(xml, "\n\n\t\t", "non-printable", "");
				xkb_keymap_key_for_each(main.keymap(), _non_printable, this);

				/* FIXME xml.append() as last operation breaks indentation */
				xml.node("dummy", [] () {});
			});

		} else if (mod == Mod::CONTROL) {
			/* generate control character map */
			append_comment(xml, "\n\n\t", _string(mod), "");
			xml.node("map", [&] ()
			{
				xml.attribute("mod2", true);

				xkb_keymap_key_for_each(main.keymap(), _control, this);

				/* FIXME xml.append() as last operation breaks indentation */
				xml.node("dummy", [] () {});
			});

		} else {

			/* generate characters depending on modifier state */
			append_comment(xml, "\n\n\t", _string(mod), "");
			xml.node("map", [&] ()
			{
				xml.attribute("mod1", (bool)(unsigned(mod) & unsigned(Mod::SHIFT)));
				xml.attribute("mod2", false);
				xml.attribute("mod3", (bool)(unsigned(mod) & unsigned(Mod::ALTGR)));
				xml.attribute("mod4", (bool)(unsigned(mod) & unsigned(Mod::CAPSLOCK)));

				xkb_keymap_key_for_each(main.keymap(), _printable, this);

				/* FIXME xml.append() as last operation breaks indentation */
				xml.node("dummy", [] () {});
			});
		}
	}
};


struct Main::Sequence
{
	struct Guard
	{
		std::vector<Keysym> &seq;

		Guard(std::vector<Keysym> &seq, Keysym keysym) : seq(seq) { seq.push_back(keysym); }
		~Guard() { seq.pop_back(); }
	};

	Main          &_main;
	Xml_generator &_xml;

	xkb_compose_state *_state { xkb_compose_state_ref(_main._compose_state) };

	void _generate(std::vector<Keysym> &seq, Keysym keysym)
	{
		Guard g(seq, keysym);

		if (seq.size() > 4) {
			::fprintf(stderr, "dead-key / compose sequence too long (max=4)\n");
			return;
		}

		xkb_compose_state_reset(_state);
		for (Keysym k : seq) xkb_compose_state_feed(_state, k.keysym);

		switch (xkb_compose_state_get_status(_state)) {
		case XKB_COMPOSE_COMPOSED: {
				xkb_keysym_t result = xkb_compose_state_get_one_sym(_state);
				unsigned     utf32  = xkb_keysym_to_utf32(result);

				if (utf32 == 0) {
					::fprintf(stderr, "skipping sequence");
					for (Keysym k : seq) fprintf(stderr, " U+%05x", k.utf32);
					::fprintf(stderr, " generating U+%04x\n", utf32);
					break;
				}

				_xml.node("sequence", [&] ()
				{
					try {
						_xml.attribute("first",  Formatted("0x%04x", seq.at(0).utf32).string());
						_xml.attribute("second", Formatted("0x%04x", seq.at(1).utf32).string());
						_xml.attribute("third",  Formatted("0x%04x", seq.at(2).utf32).string());
						_xml.attribute("fourth", Formatted("0x%04x", seq.at(3).utf32).string());
					} catch (std::out_of_range &) { }

					_xml.attribute("code", Formatted("0x%04x", utf32).string());
				});

				char comment[32];
				xkb_keysym_to_utf8(result, comment, sizeof(comment));
				append_comment(_xml, "\t", comment, "");
			} break;

		case XKB_COMPOSE_COMPOSING:
			for (Keysym k : _main._keysyms) {
				_generate(seq, k);
			}
			break;

		case XKB_COMPOSE_CANCELLED:
		case XKB_COMPOSE_NOTHING:
			break;
		}
	}

	Sequence(Main &main, Xml_generator &xml)
	:
		_main(main), _xml(xml)
	{
		append_comment(_xml, "\n\n\t", "dead-key / compose sequences", "");
		for (Keysym k : _main._keysyms) {
			/* first must be a dead/composing keysym */
			if (!k.composing) continue;

			std::vector<Keysym> seq;

			_generate(seq, k);
		}

		/* FIXME xml.append() as last operation breaks indentation */
		xml.node("dummy", [] () {});
	}

	~Sequence() { xkb_compose_state_unref(_state); }
};


char const * Main::_string(enum xkb_compose_status status)
{
    switch (status) {
    case XKB_COMPOSE_NOTHING:   return "XKB_COMPOSE_NOTHING";
    case XKB_COMPOSE_COMPOSING: return "XKB_COMPOSE_COMPOSING";
    case XKB_COMPOSE_COMPOSED:  return "XKB_COMPOSE_COMPOSED";
    case XKB_COMPOSE_CANCELLED: return "XKB_COMPOSE_CANCELLED";
    }
    return "invalid";
}


char const * Main::_string(enum xkb_compose_feed_result result)
{
    switch (result) {
    case XKB_COMPOSE_FEED_IGNORED:  return "XKB_COMPOSE_FEED_IGNORED";
    case XKB_COMPOSE_FEED_ACCEPTED: return "XKB_COMPOSE_FEED_ACCEPTED";
    }
    return "invalid";
}


void Main::_keycode_info(xkb_keycode_t keycode)
{
	for (Xkb::Mapping &m : Xkb::printable) {
		if (m.xkb != keycode) continue;

		::printf("keycode %3u:", m.xkb);
		::printf(" %-8s", m.xkb_name);
		::printf(" %-16s", Input::key_name(m.code));

		unsigned const num_levels = xkb_keymap_num_levels_for_key(_keymap, m.xkb, 0);
		::printf("\t%u levels { ", num_levels);

		for (unsigned l = 0; l < num_levels; ++l) {
			::printf(" %u:", l);

			xkb_keysym_t const *syms = nullptr;
			unsigned const num_syms = xkb_keymap_key_get_syms_by_level(_keymap, m.xkb, 0, l, &syms);

			for (unsigned s = 0; s < num_syms; ++s) {
				char buffer[7] = { 0, };
				xkb_keysym_to_utf8(syms[s], buffer, sizeof(buffer));
				::printf(" %x %s", syms[s], keysym_composing(_compose_state, syms[s])
				                            ? "COMPOSING!" : buffer);
			}
		}

		::printf(" }");
		::printf("\n");
		return;
	}
}


void Main::_keycode_xml_non_printable(Xml_generator &xml, xkb_keycode_t keycode)
{
	/* non-printable symbols with chargen entry (e.g., ENTER) */
	for (Xkb::Mapping &m : Xkb::non_printable) {
		if (m.xkb != keycode) continue;

		xml.node("key", [&] ()
		{
			xml.attribute("name",  Input::key_name(m.code));
			xml.attribute("ascii", m.ascii);
		});

		return;
	}
}


void Main::_keycode_xml_control(Xml_generator &xml, xkb_keycode_t keycode)
{
	/* chargen entry for control characters (e.g., CTRL-J) */
	static char const *desc[] {
		"SOH (start of heading)    ",
		"STX (start of text)       ",
		"ETX (end of text)         ",
		"EOT (end of transmission) ",
		"ENQ (enquiry)             ",
		"ACK (acknowledge)         ",
		"BEL '\\a' (bell)           ",
		"BS  '\\b' (backspace)      ",
		"HT  '\\t' (horizontal tab) ",
		"LF  '\\n' (new line)       ",
		"VT  '\\v' (vertical tab)   ",
		"FF  '\\f' (form feed)      ",
		"CR  '\\r' (carriage ret)   ",
		"SO  (shift out)           ",
		"SI  (shift in)            ",
		"DLE (data link escape)    ",
		"DC1 (device control 1)    ",
		"DC2 (device control 2)    ",
		"DC3 (device control 3)    ",
		"DC4 (device control 4)    ",
		"NAK (negative ack.)       ",
		"SYN (synchronous idle)    ",
		"ETB (end of trans. blk)   ",
		"CAN (cancel)              ",
		"EM  (end of medium)       ",
		"SUB (substitute)          ",
		"ESC (escape)              ",
		"FS  (file separator)      ",
		"GS  (group separator)     ",
		"RS  (record separator)    ",
		"US  (unit separator)      ",
	};
	Pressed<Input::KEY_LEFTCTRL> control(_state);
	for (Xkb::Mapping &m : Xkb::printable) {
		if (m.xkb != keycode) continue;

		xkb_keysym_t const keysym = xkb_state_key_get_one_sym(_state, keycode);
		if (keysym == XKB_KEY_NoSymbol) return;

		unsigned const utf32 = xkb_state_key_get_utf32(_state, m.xkb);
		if (!utf32 || utf32 > 0x1f) return;

		char keysym_str[32];
		xkb_keysym_get_name(keysym, keysym_str, sizeof(keysym_str));

		xml.node("key", [&] ()
		{
			xml.attribute("name", Input::key_name(m.code));
			xml.attribute("code", Formatted("0x%04x", utf32).string());
		});
		append_comment(xml, "\t",
		               Formatted("%s CTRL-%s", desc[utf32-1], keysym_str).string(),
		               "");

		return;
	}
}


void Main::_keycode_xml_printable(Xml_generator &xml, xkb_keycode_t keycode)
{
	for (Xkb::Mapping &m : Xkb::printable) {
		if (m.xkb != keycode) continue;

		Key_info key_info(_state, _compose_state, m.code);
		if (!key_info.valid()) break;

		xml.node("key", [&] ()
		{
			xml.attribute("name", Input::key_name(m.code));
			key_info.attributes(xml);
		});
		key_info.comment(xml);

		Keysym keysym { key_info.composing(), key_info.keysym(), key_info.utf32() };
		_keysyms.insert(keysym);

		return;
	}
}


void Main::_keycode_xml_printable_shift(Xml_generator &xml, xkb_keycode_t keycode)
{
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_altgr(Xml_generator &xml, xkb_keycode_t keycode)
{
	Pressed<Input::KEY_RIGHTALT> altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK> capslock(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_shift_altgr(Xml_generator &xml, xkb_keycode_t keycode)
{
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	Pressed<Input::KEY_RIGHTALT>  altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_shift_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK>   capslock(_state);
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_altgr_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK>  capslock(_state);
	Pressed<Input::KEY_RIGHTALT> altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


void Main::_keycode_xml_printable_shift_altgr_capslock(Xml_generator &xml, xkb_keycode_t keycode)
{
	Locked<Input::KEY_CAPSLOCK>   capslock(_state);
	Pressed<Input::KEY_LEFTSHIFT> shift(_state);
	Pressed<Input::KEY_RIGHTALT>  altgr(_state);
	_keycode_xml_printable(xml, keycode);
}


int Main::_generate()
{
	::printf("<!-- %s/%s/%s chargen configuration generated by xkb2ifcfg -->\n",
	         args.layout, args.variant, args.locale);

	Expanding_xml_buffer xml_buffer;

	auto generate_xml = [&] (Xml_generator &xml)
	{
		{ Map map { *this, xml, Map::Mod::NONE }; }
		{ Map map { *this, xml, Map::Mod::SHIFT }; }
		{ Map map { *this, xml, Map::Mod::CONTROL }; }
		{ Map map { *this, xml, Map::Mod::ALTGR }; }
		{ Map map { *this, xml, Map::Mod::CAPSLOCK }; }
		{ Map map { *this, xml, Map::Mod::SHIFT_ALTGR }; }
		{ Map map { *this, xml, Map::Mod::SHIFT_CAPSLOCK }; }
		{ Map map { *this, xml, Map::Mod::ALTGR_CAPSLOCK }; }
		{ Map map { *this, xml, Map::Mod::SHIFT_ALTGR_CAPSLOCK }; }

		{ Sequence sequence { *this, xml }; }
	};

	xml_buffer.generate("chargen", generate_xml);

	::puts(xml_buffer.buffer());

	return 0;
}


int Main::_dump()
{
	::printf("Dump of XKB keymap for %s/%s/%s by xkb2ifcfg\n",
	         args.layout, args.variant, args.locale);
	::puts(xkb_keymap_get_as_string(_keymap, XKB_KEYMAP_FORMAT_TEXT_V1));

	return 0;
}


int Main::_info()
{
	::printf("Simple per-key info for %s/%s/%s by xkb2ifcfg\n",
	         args.layout, args.variant, args.locale);

	auto lambda = [] (xkb_keymap *, xkb_keycode_t keycode, void *data)
	{
		reinterpret_cast<Main *>(data)->_keycode_info(keycode);
	};

	xkb_keymap_key_for_each(_keymap, lambda, this);

	return 0;
}


int Main::exec()
{
	switch (args.command) {
	case Args::Command::GENERATE: return _generate();
	case Args::Command::DUMP:     return _dump();
	case Args::Command::INFO:     return _info();
	}

	return -1;
}


Main::Main(int argc, char **argv) : args(argc, argv)
{
	_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!_context) throw Xkb_init_failed();

	_rmlvo  = { "evdev", "pc105", args.layout, args.variant, "" };
	_keymap = xkb_keymap_new_from_names(_context, &_rmlvo, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!_keymap) throw Keymap_init_failed();
	_state = xkb_state_new(_keymap);
	if (!_state) throw Keymap_init_failed();

	_compose_table = xkb_compose_table_new_from_locale(_context, args.locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
	if (!_compose_table) throw Compose_init_failed();
	_compose_state = xkb_compose_state_new(_compose_table, XKB_COMPOSE_STATE_NO_FLAGS);
	if (!_compose_state) throw Compose_init_failed();

	_numlock.construct(_state);
}


Main::~Main()
{
	_numlock.destruct();

	xkb_compose_state_unref(_compose_state);
	xkb_compose_table_unref(_compose_table);
	xkb_state_unref(_state);
	xkb_keymap_unref(_keymap);
	xkb_context_unref(_context);
}


int main(int argc, char **argv)
{
	try {
		static Main m(argc, argv);

		return m.exec();
	}
	catch (Main::Compose_init_failed &) {
		::perror("Error: compose init failed (invalid locale?)"); return -4; }
	catch (Main::Keymap_init_failed &) {
		::perror("Error: keymap init failed (invalid layout or variant?)"); return -3; }
	catch (Main::Xkb_init_failed &) {
		::perror("Error: libxkbcommon init failed"); return -2; }
	catch (...) { return -1; }
}
