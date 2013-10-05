/*
 * \brief  Ncurses-based terminal multiplexer
 * \author Norman Feske
 * \date   2013-02-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/heap.h>
#include <base/sleep.h>
#include <util/arg_string.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <os/attached_ram_dataspace.h>
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
			unsigned char  ascii = cell.ascii;

			if (ascii == 0) {
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
		virtual char const *label() const = 0;

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

	bool is_first(Entry const *entry)
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

	class Session_component : public Genode::Rpc_object<Session, Session_component>,
	                          public Registry::Entry
	{
		public:

			enum { LABEL_MAX_LEN = 128 };

		private:

			Read_buffer _read_buffer;

			Ncurses         &_ncurses;
			Ncurses::Window &_window;

			struct Label
			{
				char buf[LABEL_MAX_LEN];

				Label(char const *label)
				{
					Genode::strncpy(buf, label, sizeof(buf));
				}
			} _label;

			Session_manager &_session_manager;

			Genode::Attached_ram_dataspace _io_buffer;

			Cell_array<Char_cell>            _char_cell_array;
			Char_cell_array_character_screen _char_cell_array_character_screen;
			Terminal::Decoder                _decoder;

			Terminal::Position               _last_cursor_pos;


		public:

			/**
			 * Constructor
			 */
			Session_component(Genode::size_t   io_buffer_size,
			                  Ncurses         &ncurses,
			                  Session_manager &session_manager,
			                  char const      *label)
			:
				_ncurses(ncurses),
				_window(*ncurses.create_window(0, 1, ncurses.columns(), ncurses.lines() - 1)),
				_label(label),
				_session_manager(session_manager),
				_io_buffer(Genode::env()->ram_session(), io_buffer_size),
				_char_cell_array(ncurses.columns(), ncurses.lines() - 1,
				                 Genode::env()->heap()),
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

			void flush()
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

			void flush_all()
			{
				for (unsigned line = 0; line < _char_cell_array.num_lines(); line++)
					_char_cell_array.mark_line_as_dirty(line);

				_window.erase();
				flush();
			}

			char const *label() const
			{
				return _label.buf;
			}

			void submit_input(char c)
			{
				_read_buffer.add(c);
			}


			/********************************
			 ** Terminal session interface **
			 ********************************/

			Size size() { return Size(_char_cell_array.num_cols(),
			                          _char_cell_array.num_lines()); }

			bool avail() { return !_read_buffer.empty(); }

			Genode::size_t _read(Genode::size_t dst_len)
			{
				/* read data, block on first byte if needed */
				unsigned       num_bytes = 0;
				unsigned char *dst       = _io_buffer.local_addr<unsigned char>();
				Genode::size_t dst_size  = Genode::min(_io_buffer.size(), dst_len);
				do {
					dst[num_bytes++] = _read_buffer.get();;
				} while (!_read_buffer.empty() && num_bytes < dst_size);

				return num_bytes;
			}

			void _write(Genode::size_t num_bytes)
			{
				unsigned char *src = _io_buffer.local_addr<unsigned char>();

				for (unsigned i = 0; i < num_bytes; i++) {

					/* submit character to sequence decoder */
					_decoder.insert(src[i]);
				}
			}

			Genode::Dataspace_capability _dataspace()
			{
				return _io_buffer.cap();
			}

			void connected_sigh(Genode::Signal_context_capability sigh)
			{
				/*
				 * Immediately reflect connection-established signal to the
				 * client because the session is ready to use immediately after
				 * creation.
				 */
				Genode::Signal_transmitter(sigh).submit();
			}

			void read_avail_sigh(Genode::Signal_context_capability cap)
			{
				_read_buffer.sigh(cap);
			}

			Genode::size_t read(void *buf, Genode::size_t)  { return 0; }
			Genode::size_t write(void const *buf, Genode::size_t) { return 0; }
	};


	class Root_component : public Genode::Root_component<Session_component>
	{
		private:

			Ncurses         &_ncurses;
			Session_manager &_session_manager;

		protected:

			Session_component *_create_session(const char *args)
			{
				/*
				 * XXX read I/O buffer size from args
				 */
				Genode::size_t io_buffer_size = 4096;

				char label[Session_component::LABEL_MAX_LEN];
				Genode::Arg_string::find_arg(args, "label").string(label, sizeof(label), "<unlabeled>");

				return new (md_alloc())
					Session_component(io_buffer_size, _ncurses, _session_manager, label);
			}

		public:

			/**
			 * Constructor
			 */
			Root_component(Genode::Rpc_entrypoint &ep,
			               Genode::Allocator      &md_alloc,
			               Ncurses                &ncurses,
			               Session_manager        &session_manager)
			:
				Genode::Root_component<Session_component>(&ep, &md_alloc),
				_ncurses(ncurses),
				_session_manager(session_manager)
			{ }
	};
}


class Status_window
{
	private:

		Ncurses         &_ncurses;
		Ncurses::Window &_window;

		char _label[Terminal::Session_component::LABEL_MAX_LEN];

	public:

		Status_window(Ncurses &ncurses)
		:
			_ncurses(ncurses),
			_window(*_ncurses.create_window(0, 0, ncurses.columns(), 1))
		{
			_label[0] = 0;
		}

		~Status_window()
		{
			_ncurses.destroy_window(&_window);
		}

		void label(char const *label)
		{
			Genode::strncpy(_label, label, sizeof(_label));

			_window.erase();
			_window.move_cursor(0, 0);
			_window.print_char('[', false, false);

			unsigned const max_columns = _ncurses.columns() - 2;
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

		/**
		 * State tracker for escape sequences within user input
		 *
		 * This tracker is used to decode special keys (e.g., cursor keys).
		 */
		struct Seq_tracker
		{
			enum State { INIT, GOT_ESC, GOT_FIRST } state;
			char normal, first, second;
			bool sequence_complete;

			Seq_tracker() : state(INIT), sequence_complete(false) { }

			void input(char c)
			{
				switch (state) {
				case INIT:
					if (c == 27)
						state = GOT_ESC;
					else
						normal = c;
					sequence_complete = false;
					break;

				case GOT_ESC:
					first = c;
					state = GOT_FIRST;
					break;

				case GOT_FIRST:
					second = c;
					state = INIT;
					sequence_complete = true;
					break;
				}
			}

			bool is_normal() const { return state == INIT && !sequence_complete; }

			bool _is_normal(char c) const { return is_normal() && normal == c; }

			bool _fn_complete(char match_first, char match_second) const
			{
				return sequence_complete
				    && first  == match_first
				    && second == match_second;
			}

			bool is_key_up() const {
				return _fn_complete(91, 65) || _is_normal('k'); }

			bool is_key_down() const {
				return _fn_complete(91, 66) || _is_normal('j'); }
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

		void flush() { }

		void flush_all()
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

				char const *label = entry->label();
				for (unsigned j = 0; j < (max_columns - padding) && label[j]; j++)
					_window.print_char(label[j], highlight, highlight);
			}

			_ncurses.cursor_visible(false);
			_window.refresh();
		}

		char const *label() const { return "-"; }

		void submit_input(char c)
		{
			_seq_tracker.input(c);

			if (_seq_tracker.is_key_up()) {
				if (_selected_idx > 0)
					_selected_idx--;
				flush_all();
			}

			if (_seq_tracker.is_key_down()) {
				if (_selected_idx < _max_idx)
					_selected_idx++;
				flush_all();
			}

			/*
			 * Detect selection of menu entry via [enter]
			 */
			if (_seq_tracker.is_normal() && _seq_tracker.normal == 13) {

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
	if (_registry.is_first(&_menu))
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


/*******************
 ** Input handler **
 *******************/

struct Input_handler
{
	GENODE_RPC(Rpc_handle_input, void, handle);
	GENODE_RPC_INTERFACE(Rpc_handle_input);
};


struct Input_handler_component : Genode::Rpc_object<Input_handler,
                                                    Input_handler_component>
{
	User_input      &_user_input;
	Session_manager &_session_manager;


	Input_handler_component(User_input      &user_input,
	                        Session_manager &session_manager)
	:
		_user_input(user_input),
		_session_manager(session_manager)
	{
		_session_manager.activate_menu();
	}

	void handle()
	{
		for (;;) {
			int c = _user_input.read_character();
			if (c == -1)
				break;

			/*
			 * Quirk needed when using 'qemu -serial stdio'. In this case,
			 * backspace is wrongly reported as 127.
			 */
			if (c == 127)
				c = 8;

			/*
			 * Handle C-y by switching to the menu
			 */
			enum { KEYCODE_C_X = 24 };
			if (c == KEYCODE_C_X) {
				_session_manager.activate_menu();
			} else {
				_session_manager.submit_input(c);
			}
		}

		_session_manager.update_ncurses_screen();
	}
};


int main(int, char **)
{
	using namespace Genode;

	printf("--- terminal_mux service started ---\n");

	static Cap_connection cap;

	/* initialize entry point that serves the root interface */
	enum { STACK_SIZE = sizeof(addr_t)*4096 };
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "terminal_mux_ep");

	static Sliced_heap sliced_heap(env()->ram_session(), env()->rm_session());

	static Registry      registry;
	static Ncurses       ncurses;
	static Status_window status_window(ncurses);
	static Menu          menu(ncurses, registry, status_window);

	registry.add(&menu);

	static User_input      user_input(ncurses);
	static Session_manager session_manager(ncurses, registry, status_window, menu);

	/* create root interface for service */
	static Terminal::Root_component root(ep, sliced_heap, ncurses, session_manager);

	static Input_handler_component input_handler(user_input, session_manager);
	Capability<Input_handler> input_handler_cap = ep.manage(&input_handler);

	/* announce service at our parent */
	env()->parent()->announce(ep.manage(&root));

	while (1) {
		static Timer::Connection timer;
		timer.msleep(10);

		input_handler_cap.call<Input_handler::Rpc_handle_input>();
	}
	return 0;
}
