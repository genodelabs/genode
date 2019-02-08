/*
 * \brief  Ncurses-based terminal multiplexer
 * \author Norman Feske
 * \date   2013-02-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/env.h>
#include <base/attached_ram_dataspace.h>
#include <base/session_label.h>
#include <libc/component.h>
#include <libc/allocator.h>
#include <util/arg_string.h>
#include <root/component.h>
#include <timer_session/connection.h>

/* terminal includes */
#include <terminal/decoder.h>
#include <terminal/types.h>
#include <terminal/read_buffer.h>
#include <terminal/char_cell_array_character_screen.h>
#include <terminal_session/terminal_session.h>

/* local includes */
#include <ncurses_cxx.h>


/*
 * Convert character array into ncurses window
 */
static void convert_char_array_to_window(Cell_array<Char_cell> *cell_array,
                                         Ncurses::Window &window)
{
	for (unsigned line = 0; line < cell_array->num_lines(); line++) {

		if (!cell_array->line_dirty(line))
			continue;

		window.move_cursor(0, line);
		for (unsigned column = 0; column < cell_array->num_cols(); column++) {

			Char_cell      cell  = cell_array->get_cell(column, line);
			unsigned char  ascii = cell.codepoint().value;

			if (ascii == 0 || ascii & 0x80) {
				window.print_char(' ', false, false);
				continue;
			}

			/* XXX add color */
			window.print_char(ascii, cell.highlight(), cell.inverse());
		}
	}
}


/**
 * Registry of clients of the multiplexer
 */
struct Registry
{
	struct Entry : Genode::List<Entry>::Element
	{
		/**
		 * Flush pending drawing operations
		 */
		virtual void flush() = 0;

		/**
		 * Redraw and flush complete entry
		 */
		virtual void flush_all() = 0;

		/**
		 * Return session label
		 */
		virtual Genode::Session_label const & label() const = 0;

		/**
		 * Submit character into entry
		 */
		virtual void submit_input(char c) = 0;
	};

	/**
	 * List of existing terminal sessions
	 *
	 * The first entry of the list has the current focus
	 */
	Genode::List<Entry> _list;

	/**
	 * Lookup entry at specified index
	 *
	 * \return entry, or 0 if index is out of range
	 */
	Entry *entry_at(unsigned index)
	{
		Entry *curr = _list.first();
		for (; curr && index--; curr = curr->next());
		return curr;
	}

	void add(Entry *entry)
	{
		/*
		 * Always insert new entry at the second position. The first is
		 * occupied by the current focused entry.
		 */
		Entry *first = _list.first();
		if (first)
			_list.remove(first);

		_list.insert(entry);

		if (first)
			_list.insert(first);
	}

	void remove(Entry *entry)
	{
		_list.remove(entry);
	}

	void to_front(Entry *entry)
	{
		if (!entry) return;

		/* make entry the first one of the list */
		_list.remove(entry);
		_list.insert(entry);
	}

	bool first(Entry const *entry)
	{
		return _list.first() == entry;
	}
};


class Status_window;
class Menu;


class Session_manager
{
	private:

		Ncurses       &_ncurses;
		Registry      &_registry;
		Status_window &_status_window;
		Menu          &_menu;

		/**
		 * Update menu if it has the current focus
		 */
		void _refresh_menu();

	public:

		Session_manager(Ncurses &ncurses, Registry &registry,
		                Status_window &status_window, Menu &menu);

		void activate_menu();
		void submit_input(char c);
		void update_ncurses_screen();
		void add(Registry::Entry *entry);
		void remove(Registry::Entry *entry);
};


namespace Terminal {
	class Session_component;
	class Root_component;
}

class Terminal::Session_component : public Genode::Rpc_object<Session, Session_component>,
                                    public Registry::Entry
{
	private:

		Genode::Env  &_env;

		Read_buffer _read_buffer;

		Ncurses         &_ncurses;
		Ncurses::Window &_window;

		Genode::Session_label const _label;

		Session_manager &_session_manager;

		Genode::Attached_ram_dataspace _io_buffer;

		Cell_array<Char_cell>            _char_cell_array;
		Char_cell_array_character_screen _char_cell_array_character_screen;
		Terminal::Decoder                _decoder;

		Terminal::Position               _last_cursor_pos;

	public:

		Session_component(Genode::size_t               io_buffer_size,
		                  Ncurses                     &ncurses,
		                  Session_manager             &session_manager,
		                  Genode::Session_label const &label,
		                  Genode::Env                 &env,
		                  Genode::Allocator           &heap)
		:
			_env(env),
			_ncurses(ncurses),
			_window(*ncurses.create_window(0, 1, ncurses.columns(), ncurses.lines() - 1)),
			_label(label),
			_session_manager(session_manager),
			_io_buffer(_env.ram(), _env.rm(), io_buffer_size),
			_char_cell_array(ncurses.columns(), ncurses.lines() - 1, heap),
			_char_cell_array_character_screen(_char_cell_array),
			_decoder(_char_cell_array_character_screen)
		{
			_session_manager.add(this);
		}

		~Session_component()
		{
			_session_manager.remove(this);
			_ncurses.destroy_window(&_window);
		}


		/*******************************
		 ** Registry::Entry interface **
		 *******************************/

		void flush() override
		{
			convert_char_array_to_window(&_char_cell_array, _window);

			int first_dirty_line =  10000,
			    last_dirty_line  = -10000;

			for (int line = 0; line < (int)_char_cell_array.num_lines(); line++) {
				if (!_char_cell_array.line_dirty(line)) continue;

				first_dirty_line = Genode::min(line, first_dirty_line);
				last_dirty_line  = Genode::max(line, last_dirty_line);

				_char_cell_array.mark_line_as_clean(line);
			}

			Terminal::Position cursor_pos =
				_char_cell_array_character_screen.cursor_pos();
			_window.move_cursor(cursor_pos.x, cursor_pos.y);

			_window.refresh();
		}

		void flush_all() override
		{
			for (unsigned line = 0; line < _char_cell_array.num_lines(); line++)
				_char_cell_array.mark_line_as_dirty(line);

			_window.erase();
			flush();
		}

		Genode::Session_label const & label() const override { return _label; }

		void submit_input(char c) override { _read_buffer.add(c); }


		/********************************
		 ** Terminal session interface **
		 ********************************/

		Size size() override { return Size(_char_cell_array.num_cols(),
		                          _char_cell_array.num_lines()); }

		bool avail() override { return !_read_buffer.empty(); }

		Genode::size_t _read(Genode::size_t dst_len)
		{
			/* read data, block on first byte if needed */
			unsigned       num_bytes = 0;
			unsigned char *dst       = _io_buffer.local_addr<unsigned char>();
			Genode::size_t dst_size  = Genode::min(_io_buffer.size(), dst_len);
			do {
				dst[num_bytes++] = _read_buffer.get();
			} while (!_read_buffer.empty() && num_bytes < dst_size);

			return num_bytes;
		}

		Genode::size_t _write(Genode::size_t num_bytes)
		{
			unsigned char *src = _io_buffer.local_addr<unsigned char>();

			Terminal::Character character;
			for (unsigned i = 0; i < num_bytes; i++) {

				/* submit character to sequence decoder */
				character.value = src[i];
				_decoder.insert(character);
			}

			return num_bytes;
		}

		Genode::Dataspace_capability _dataspace()
		{
			return _io_buffer.cap();
		}

		void connected_sigh(Genode::Signal_context_capability sigh) override
		{
			/*
			 * Immediately reflect connection-established signal to the
			 * client because the session is ready to use immediately after
			 * creation.
			 */
			Genode::Signal_transmitter(sigh).submit();
		}

		void read_avail_sigh(Genode::Signal_context_capability cap) override
		{
			_read_buffer.sigh(cap);
		}

		void size_changed_sigh(Genode::Signal_context_capability cap) override
		{ }

		Genode::size_t read(void *buf, Genode::size_t) override  { return 0; }
		Genode::size_t write(void const *buf, Genode::size_t) override { return 0; }
};


class Terminal::Root_component : public Genode::Root_component<Session_component>
{
	private:

		Genode::Env     &_env;
		Ncurses         &_ncurses;
		Session_manager &_session_manager;

		/*
		 * FIXME The heap is shared between all clients. The allocator should
		 * be moved into the session component but this increases per-session
		 * RAM costs significantly, which would break all connections to this
		 * server.
		 */
		Genode::Allocator &_heap;

	protected:

		Session_component *_create_session(const char *args)
		{
			/*
			 * XXX read I/O buffer size from args
			 */
			Genode::size_t io_buffer_size = 4096;

			return new (md_alloc())
				Session_component(io_buffer_size, _ncurses, _session_manager,
				                  Genode::label_from_args(args), _env, _heap);
		}

	public:

		/**
		 * Constructor
		 */
		Root_component(Genode::Env       &env,
		               Genode::Allocator &heap,
		               Ncurses           &ncurses,
		               Session_manager   &session_manager)
		:
			Genode::Root_component<Session_component>(env.ep(), heap),
			_env(env),
			_ncurses(ncurses),
			_session_manager(session_manager),
			_heap(heap)
		{ }
};


class Status_window
{
	private:

		Ncurses         &_ncurses;
		Ncurses::Window &_window;

	public:

		Status_window(Ncurses &ncurses)
		:
			_ncurses(ncurses),
			_window(*_ncurses.create_window(0, 0, ncurses.columns(), 1))
		{ }

		~Status_window()
		{
			_ncurses.destroy_window(&_window);
		}

		void label(Genode::Session_label const &label)
		{
			_window.erase();
			_window.move_cursor(0, 0);
			_window.print_char('[', false, false);

			unsigned const max_columns = _ncurses.columns() - 2;
			char const *_label = label.string();
			for (unsigned i = 0; i < max_columns && _label[i]; i++)
				_window.print_char(_label[i], false, false);

			_window.print_char(']', false, false);
			_window.refresh();
		}
};


class Menu : public Registry::Entry
{
	private:

		Ncurses         &_ncurses;
		Ncurses::Window &_window;
		Status_window   &_status_window;
		Registry        &_registry;
		unsigned         _selected_idx;
		unsigned         _max_idx;

		Genode::Session_label const _label { "-" };

		/**
		 * State tracker for escape sequences within user input
		 *
		 * This tracker is used to decode special keys (e.g., cursor keys).
		 */
		struct Seq_tracker
		{
			enum State { INIT, GOT_ESC, GOT_FIRST } _state;
			char _normal, _first, _second;
			bool _sequence_complete;

			Seq_tracker() : _state(INIT), _sequence_complete(false) { }

			void input(char c)
			{
				switch (_state) {
				case INIT:
					if (c == 27)
						_state = GOT_ESC;
					else
						_normal = c;
					_sequence_complete = false;
					break;

				case GOT_ESC:
					_first = c;
					_state = GOT_FIRST;
					break;

				case GOT_FIRST:
					_second = c;
					_state = INIT;
					_sequence_complete = true;
					break;
				}
			}

			bool normal() const { return _state == INIT && !_sequence_complete; }

			char normal_char() const { return _normal; }

			bool _normal_matches(char c) const { return normal() && _normal == c; }

			bool _fn_complete(char match_first, char match_second) const
			{
				return _sequence_complete
				    && _first  == match_first
				    && _second == match_second;
			}

			bool key_up() const {
				return _fn_complete(91, 65) || _normal_matches('k'); }

			bool key_down() const {
				return _fn_complete(91, 66) || _normal_matches('j'); }
		};

		Seq_tracker _seq_tracker;

	public:

		Menu(Ncurses &ncurses, Registry &registry, Status_window &status_window)
		:
			_ncurses(ncurses),
			_window(*_ncurses.create_window(0, 1,
			                                ncurses.columns(),
			                                ncurses.lines() - 1)),
			_status_window(status_window),
			_registry(registry),
			_selected_idx(0),
			_max_idx(0)
		{ }

		~Menu()
		{
			_ncurses.destroy_window(&_window);
		}

		void reset_selection() { _selected_idx = 0; }

		void flush() override { }

		void flush_all() override
		{
			_window.erase();

			unsigned const max_columns = _ncurses.columns() - 1;

			for (unsigned i = 0; i < _ncurses.lines() - 2; i++) {

				Registry::Entry *entry = _registry.entry_at(i + 1);
				if (!entry) {
					_max_idx = i - 1;
					break;
				}

				bool const highlight = (i == _selected_idx);
				if (highlight)
					_window.horizontal_line(i + 1);

				unsigned const padding = 2;
				_window.move_cursor(padding, 1 + i);

				char const *label = entry->label().string();
				for (unsigned j = 0; j < (max_columns - padding) && label[j]; j++)
					_window.print_char(label[j], highlight, highlight);
			}

			_ncurses.cursor_visible(false);
			_window.refresh();
		}

		Genode::Session_label const & label() const { return _label; }

		void submit_input(char c)
		{
			_seq_tracker.input(c);

			if (_seq_tracker.key_up()) {
				if (_selected_idx > 0)
					_selected_idx--;
				flush_all();
			}

			if (_seq_tracker.key_down()) {
				if (_selected_idx < _max_idx)
					_selected_idx++;
				flush_all();
			}

			/*
			 * Detect selection of menu entry via [enter]
			 */
			if (_seq_tracker.normal() && _seq_tracker.normal_char() == 13) {

				Entry *entry = _registry.entry_at(_selected_idx + 1);
				if (entry) {
					_registry.to_front(entry);

					/* update status window */
					_status_window.label(_registry.entry_at(0)->label());

					_ncurses.cursor_visible(true);
					entry->flush_all();
				}
			}
		}
};


struct User_input
{
	Ncurses &_ncurses;

	User_input(Ncurses &ncurses) : _ncurses(ncurses) { }

	int read_character()
	{
		return _ncurses.read_character();
	}
};


/************************************
 ** Session manager implementation **
 ************************************/

Session_manager::Session_manager(Ncurses &ncurses, Registry &registry,
                                 Status_window &status_window, Menu &menu)
:
	_ncurses(ncurses), _registry(registry), _status_window(status_window),
	_menu(menu)
{ }


void Session_manager::_refresh_menu()
{
	if (_registry.first(&_menu))
		activate_menu();
}


void Session_manager::activate_menu()
{
	_menu.reset_selection();
	_registry.to_front(&_menu);
	_status_window.label(_menu.label());
	_ncurses.clear_ok();
	_menu.flush_all();
}


void Session_manager::submit_input(char c)
{
	Registry::Entry *focused = _registry.entry_at(0);
	if (focused)
		focused->submit_input(c);
}


void Session_manager::update_ncurses_screen()
{
	Registry::Entry *focused = _registry.entry_at(0);
	if (focused)
		focused->flush();
	_ncurses.do_update();
}


void Session_manager::add(Registry::Entry *entry)
{
	_registry.add(entry);
	_refresh_menu();
}


void Session_manager::remove(Registry::Entry *entry)
{
	_registry.remove(entry);
	_refresh_menu();
}


/***************
 ** Component **
 ***************/

struct Main
{
	Genode::Env &env;

	Libc::Allocator heap;

	Registry      registry;
	Ncurses       ncurses       { heap };
	Status_window status_window { ncurses };
	Menu          menu          { ncurses, registry, status_window };

	User_input      user_input      { ncurses };
	Session_manager session_manager { ncurses, registry, status_window, menu };

	Terminal::Root_component root { env, heap, ncurses, session_manager };

	Timer::Connection timer { env };

	Genode::Signal_handler<Main> timer_handler { env.ep(), *this, &Main::handle_timer };

	Main(Genode::Env &env) : env(env)
	{
		Genode::log("--- terminal_mux service started ---");

		registry.add(&menu);
		session_manager.activate_menu();

		env.parent().announce(env.ep().manage(root));

		timer.sigh(timer_handler);
		timer.trigger_periodic(10*1000);
	}

	void handle_timer()
	{
		for (;;) {
			int c = user_input.read_character();
			if (c == -1)
				break;

			/*
			 * Quirk needed when using 'qemu -serial stdio'. In this case,
			 * backspace is wrongly reported as 127.
			 */
			if (c == 127)
				c = 8;

			/*
			 * Handle C-x by switching to the menu
			 */
			enum { KEYCODE_C_X = 24 };
			if (c == KEYCODE_C_X) {
				session_manager.activate_menu();
			} else {
				session_manager.submit_input(c);
			}
		}

		session_manager.update_ncurses_screen();
	}
};


void Libc::Component::construct(Libc::Env &env) { static Main inst(env); }
