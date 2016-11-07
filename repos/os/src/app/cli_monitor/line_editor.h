/*
 * \brief  Line editor for command-line interfaces
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LINE_EDITOR_H_
#define _LINE_EDITOR_H_

/* Genode includes */
#include <base/printf.h>
#include <terminal_session/connection.h>
#include <util/string.h>
#include <util/list.h>
#include <util/token.h>
#include <util/misc_math.h>
#include <base/snprintf.h>

using Genode::List;
using Genode::max;
using Genode::strlen;
using Genode::strncpy;
using Genode::snprintf;
using Genode::strcmp;
using Genode::size_t;
using Genode::off_t;


struct Completable
{
	typedef Genode::String<64>  Name;
	typedef Genode::String<160> Short_help;

	Name       const _name;
	Short_help const _short_help;

	Name       name()        const { return _name; }
	Short_help short_help()  const { return _short_help; }

	Completable(char const *name, char const *short_help)
	: _name(name), _short_help(short_help) { }
};


/**
 * Representation of normal command-line argument
 */
struct Argument : Completable
{
	Argument(char const *name, char const *short_help)
	: Completable(name, short_help) { }

	char const *name_suffix() const { return ""; }
};


/**
 * Representation of a parameter of the form '--tag value'
 */
struct Parameter : List<Parameter>::Element, Completable
{
	enum Type { IDENT, NUMBER, VOID };

	Type const _type;

	Parameter(char const *name, Type type, char const *short_help)
	:
		Completable(name, short_help), _type(type)
	{ }

	bool needs_value() const { return _type != VOID; }

	char const *name_suffix() const
	{
		switch (_type) {
		case IDENT:  return "<identifier>";
		case NUMBER: return "<number>";
		case VOID:   return "";
		}
		return "";
	}
};


struct Command_line;


/**
 * Representation of a command that can have arguments and parameters
 */
struct Command : List<Command>::Element, Completable
{
	List<Parameter> _parameters;

	/**
	 * Functor that takes a command 'Argument' object as argument
	 */
	struct Argument_fn
	{
		virtual void operator () (Argument const &) const = 0;
	};

	Command(char const *name, char const *short_help)
	: Completable(name, short_help) { }

	void add_parameter(Parameter *par) { _parameters.insert(par); }

	char const *name_suffix() const { return ""; }

	List<Parameter> &parameters() { return _parameters; }

	virtual void execute(Command_line &, Terminal::Session &terminal) = 0;

	/**
	 * Command-specific support for 'for_each_argument'
	 */
	virtual void _for_each_argument(Argument_fn const &fn) const { };

	/**
	 * Execute functor 'fn' for each command argument
	 */
	template <typename FN>
	void for_each_argument(FN const &fn) const
	{
		struct _Fn : Argument_fn
		{
			FN const &fn;
			void operator () (Argument const &arg) const override { fn(arg); }
			_Fn(FN const &fn) : fn(fn) { }
		} _fn(fn);

		_for_each_argument(_fn);
	}
};


struct Command_registry : List<Command> { };


/**
 * Scanner policy that accepts '-', '.' and '_' as valid identifier characters
 */
struct Scanner_policy
{
	static bool identifier_char(char c, unsigned i)
	{
		return Genode::is_letter(c) || (c == '_') || (c == '-') || (c == '.')
		                            || (i && Genode::is_digit(c));
	}
};


typedef Genode::Token<Scanner_policy> Token;


/**
 * State machine used for sequentially parsing command-line arguments
 */
struct Argument_tracker
{
	private:

		Command &_command;

		enum State { EXPECT_COMMAND, EXPECT_SPACE_BEFORE_ARG, EXPECT_ARG,
		             EXPECT_SPACE_BEFORE_VAL, EXPECT_VAL, INVALID };

		State _state;

		/**
		 * Return true if there is exactly one complete match and no additional
		 * partial matches
		 */
		static bool _one_matching_argument(char const *str, size_t str_len,
		                                   Command const &command)
		{
			unsigned complete_cnt = 0, partial_cnt = 0;

			auto argument_fn = [&] (Argument const &arg) {

				if (strcmp(arg.name().string(), str, str_len) == 0) {
					partial_cnt++;

					if (strlen(arg.name().string()) == str_len)
						complete_cnt++;
				}
			};

			command.for_each_argument(argument_fn);

			return partial_cnt == 1 && complete_cnt == 1;
		}

	public:

		Argument_tracker(Command &command)
		: _command(command), _state(EXPECT_COMMAND) { }

		template <typename T>
		static T *lookup(char const *str, size_t str_len,
		                     List<T> &list)
		{
			Token tag(str, str_len);
			for (T *curr = list.first(); curr; curr = curr->next())
				if (strcmp(tag.start(), curr->name().string(), tag.len()) == 0
				 && strlen(curr->name().string()) == tag.len())
					return curr;

			return 0;
		}

		template <typename T>
		static T *lookup(Token token, List<T> &list)
		{
			return lookup(token.start(), token.len(), list);
		}

		void supply_token(Token token, bool token_may_be_incomplete = false)
		{
			switch (_state) {

			case INVALID: break;

			case EXPECT_COMMAND:

				if (token.type() == Token::IDENT) {
					_state = EXPECT_SPACE_BEFORE_ARG;
					break;
				}
				_state = INVALID;
				break;

			case EXPECT_SPACE_BEFORE_ARG:

				if (token.type() == Token::WHITESPACE)
					_state = EXPECT_ARG;
				break;

			case EXPECT_ARG:

				if (token.type() == Token::IDENT) {

					Parameter *parameter =
						lookup(token.start(), token.len(), _command.parameters());

					if (parameter && parameter->needs_value()) {
						_state = EXPECT_SPACE_BEFORE_VAL;
						break;
					}

					if (!token_may_be_incomplete
					 || _one_matching_argument(token.start(), token.len(), _command))
						_state = EXPECT_SPACE_BEFORE_ARG;
				}
				break;

			case EXPECT_SPACE_BEFORE_VAL:

				if (token.type() == Token::WHITESPACE)
					_state = EXPECT_VAL;
				break;

			case EXPECT_VAL:

				if (token.type() == Token::IDENT
				 || token.type() == Token::NUMBER) {

					_state = EXPECT_SPACE_BEFORE_ARG;
				}
				break;
			}
		}

		bool valid()        const { return _state != INVALID; }
		bool expect_arg()   const { return _state == EXPECT_ARG; }
		bool expect_space() const { return _state == EXPECT_SPACE_BEFORE_ARG
		                                || _state == EXPECT_SPACE_BEFORE_VAL; }
};


/**
 * Editing and completion logic
 */
class Line_editor
{
	private:

		char        const *_prompt;
		size_t      const  _prompt_len;
		char      * const  _buf;
		size_t      const  _buf_size;
		unsigned           _cursor_pos;
		Terminal::Session &_terminal;
		Command_registry  &_commands;
		bool               _complete;

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

			bool _fn_complete(char match_first, char match_second) const
			{
				return _sequence_complete
				    && _first  == match_first
				    && _second == match_second;
			}

			bool key_up()     const { return _fn_complete(91, 65); }
			bool key_down()   const { return _fn_complete(91, 66); }
			bool key_right()  const { return _fn_complete(91, 67); }
			bool key_left()   const { return _fn_complete(91, 68); }
			bool key_delete() const { return _fn_complete(91, 51); }
		};

		Seq_tracker _seq_tracker;

		void _write(char c) { _terminal.write(&c, sizeof(c)); }

		void _write(char const *s) { _terminal.write(s, strlen(s)); } 

		void _write_spaces(unsigned num)
		{
			for (unsigned i = 0; i < num; i++)
				_write(' ');
		}

		void _write_newline() { _write(10); }

		void _clear_until_end_of_line() { _write("\e[K "); }

		void _move_cursor_to(unsigned pos)
		{
			char seq[10];
			snprintf(seq, sizeof(seq), "\e[%ldG", pos + _prompt_len);
			_write(seq);
		}

		void _delete_character()
		{
			strncpy(&_buf[_cursor_pos], &_buf[_cursor_pos + 1], _buf_size);

			_move_cursor_to(_cursor_pos);
			_write(&_buf[_cursor_pos]);
			_clear_until_end_of_line();
			_move_cursor_to(_cursor_pos);
		}

		void _insert_character(char c)
		{
			/* insert regular character */
			if (_cursor_pos >= _buf_size - 1)
				return;

			/* make room in the buffer */
			for (unsigned i = _buf_size - 1; i > _cursor_pos; i--)
				_buf[i] = _buf[i - 1];
			_buf[_cursor_pos] = c;

			/* update terminal */
			_write(&_buf[_cursor_pos]);
			_cursor_pos++;
			_move_cursor_to(_cursor_pos);
		}

		void _fresh_prompt()
		{
			_write(_prompt);
			_write(_buf);
			_move_cursor_to(_cursor_pos);
		}

		void _handle_key()
		{
			enum { BACKSPACE       =  8,
			       TAB             =  9,
			       LINE_FEED       = 10,
			       CARRIAGE_RETURN = 13 };

			if (_seq_tracker.key_left()) {
				if (_cursor_pos > 0) {
					_cursor_pos--;
					_write(BACKSPACE);
				}
				return;
			}

			if (_seq_tracker.key_right()) {
				if (_cursor_pos < strlen(_buf)) {
					_cursor_pos++;
					_move_cursor_to(_cursor_pos);
				}
				return;
			}

			if (_seq_tracker.key_delete())
				_delete_character();

			if (!_seq_tracker.normal())
				return;

			char const c = _seq_tracker.normal_char();

			if (c == TAB) {
				_perform_completion();
				return;
			}

			if (c == CARRIAGE_RETURN || c == LINE_FEED) {
				if (strlen(_buf) > 0) {
					_write(LINE_FEED);
					_complete = true;
				}
				return;
			}

			if (c == BACKSPACE) {
				if (_cursor_pos > 0) {
					_cursor_pos--;
					_delete_character();
				}
				return;
			}

			if (c == 126)
				return;

			_insert_character(c);
		}

		template <typename COMPLETABLE>
		COMPLETABLE *_lookup_matching(char const *str, size_t str_len,
		                              List<COMPLETABLE> &list)
		{
			Token tag(str, str_len);
			COMPLETABLE *curr = list.first();
			for (; curr; curr = curr->next()) {
				if (strcmp(tag.start(), curr->name(), tag.len()) == 0
				 && strlen(curr->name()) == tag.len())
					return curr;
			}
			return 0;
		}

		Command *_lookup_matching_command()
		{
			Token cmd(_buf, _cursor_pos);
			for (Command *curr = _commands.first(); curr; curr = curr->next())
				if (strcmp(cmd.start(), curr->name().string(), cmd.len()) == 0
				 && _cursor_pos > cmd.len())
					return curr;
			return 0;
		}

		template <typename T>
		unsigned _num_partial_matches(char const *str, size_t str_len, List<T> &list)
		{
			Token token(str, str_len);

			unsigned num_partial_matches = 0;
			for (T *curr = list.first(); curr; curr = curr->next()) {
				if (strcmp(token.start(), curr->name().string(), token.len()) != 0)
					continue;

				num_partial_matches++;
			}
			return num_partial_matches;
		}

		unsigned _num_matching_arguments(char const *str, size_t str_len,
		                                 Command const &command) const
		{
			unsigned num_matches = 0;

			auto argument_fn = [&] (Argument const &arg) {

				if (strcmp(arg.name().string(), str, str_len) == 0)
					num_matches++;
			};

			command.for_each_argument(argument_fn);

			return num_matches;
		}

		/**
		 * Determine the name-column width of list of partial matches
		 */
		template <typename T>
		size_t _width_of_partial_matches(char const *str, size_t str_len,
		                                 List<T> &list)
		{
			Token token(str, str_len);

			size_t max_name_len = 0;
			for (T *curr = list.first(); curr; curr = curr->next()) {
				if (strcmp(token.start(), curr->name().string(), token.len()) != 0)
					continue;

				size_t const name_len = strlen(curr->name().string())
				                      + strlen(curr->name_suffix());
				max_name_len = max(max_name_len, name_len);
			}
			return max_name_len;
		}

		unsigned _width_of_matching_arguments(char const *str, size_t str_len,
		                                      Command const &command) const
		{
			size_t max_name_len = 0;

			auto argument_fn = [&] (Argument const &arg) {

				if (strcmp(arg.name().string(), str, str_len) == 0) {
					size_t const name_len = strlen(arg.name().string());
					if (name_len > max_name_len)
						max_name_len = name_len;
				}
			};

			command.for_each_argument(argument_fn);

			return max_name_len;
		}

		template <typename T>
		char const *_any_partial_match_name(char const *str, size_t str_len,
		                                    List<T> &list)
		{
			Token token(str, str_len);

			for (T *curr = list.first(); curr; curr = curr->next())
				if (strcmp(token.start(), curr->name().string(), token.len()) == 0)
					return curr->name().string();

			return 0;
		}

		Argument::Name _any_matching_argument(char const *str, size_t str_len,
		                                      Command const &command) const
		{
			Argument::Name name;

			auto argument_fn = [&] (Argument const &arg) {

				if (strcmp(arg.name().string(), str, str_len) == 0)
					name = arg.name();
			};

			command.for_each_argument(argument_fn);

			return name;
		}

		template <typename T>
		void _list_partial_matches(char const *str, size_t str_len,
		                           unsigned pad, List<T> &list)
		{
			Token token(str, str_len);

			for (T *curr = list.first(); curr; curr = curr->next()) {
				if (strcmp(token.start(), curr->name().string(), token.len()) != 0)
					continue;

				_write_newline();
				_write_spaces(2);
				_write(curr->name().string());
				_write_spaces(1);
				_write(curr->name_suffix());

				/* pad short help with whitespaces */
				size_t const name_len = strlen(curr->name().string())
				                      + strlen(curr->name_suffix());
				_write_spaces(pad + 3 - name_len);
				_write(curr->short_help().string());
			}
		}

		void _list_matching_arguments(char const *str, size_t str_len,
		                              unsigned pad, Command const &command)
		{
			auto argument_fn = [&] (Argument const &arg) {

				if (strcmp(arg.name().string(), str, str_len) == 0) {

					_write_newline();
					_write_spaces(2);
					_write(arg.name().string());
					_write_spaces(1);
					_write(arg.name_suffix());

					/* pad short help with whitespaces */
					size_t const name_len = strlen(arg.name().string())
					                      + strlen(arg.name_suffix());
					_write_spaces(pad + 3 - name_len);
					_write(arg.short_help().string());
				}
			};

			command.for_each_argument(argument_fn);
		}

		template <typename T>
		void _do_completion(char const *str, size_t str_len, List<T> &list)
		{
			Token token(str, str_len);

			/* look up completable token */
			T *partial_match = 0;
			for (T *curr = list.first(); curr; curr = curr->next()) {
				if (strcmp(token.start(), curr->name().string(), token.len()) == 0) {
					partial_match = curr;
					break;
				}
			}

			if (!partial_match)
				return;

			for (unsigned i = token.len(); i < strlen(partial_match->name().string()); i++)
				_insert_character(partial_match->name().string()[i]);

			_insert_character(' ');
		}

		void _do_argument_completion(char const *str, size_t str_len,
		                             Command const &command)
		{
			Argument::Name partial_match;

			auto argument_fn = [&] (Argument const &arg) {

				if (strcmp(arg.name().string(), str, str_len) == 0)
					partial_match = arg.name();
			};

			command.for_each_argument(argument_fn);

			for (unsigned i = str_len; i < strlen(partial_match.string()); i++)
				_insert_character(partial_match.string()[i]);

			_insert_character(' ');
		}

		void _complete_argument(char const *str, size_t str_len, Command &command)
		{
			unsigned const matching_parameters =
				_num_partial_matches(str, str_len, command.parameters());

			unsigned const matching_arguments =
				_num_matching_arguments(str, str_len, command);

			/* matches are ambiguous */
			if (matching_arguments + matching_parameters > 1) {

				/*
				 * Try to complete additional characters that are common among
				 * all matches.
				 */
				char buf[Completable::Name::size()];
				strncpy(buf, str, Genode::min(sizeof(buf), str_len + 1));

				/* pick any representative as a template to take characters from */
				char const *name = _any_partial_match_name(str, str_len, command.parameters());
				Argument::Name arg_name;
				if (!name) {
					arg_name = _any_matching_argument(str, str_len, command);
					if (strlen(arg_name.string()))
						name = arg_name.string();
				}

				size_t i = str_len;
				for (; (i < sizeof(buf) - 1) && (i < strlen(name)); i++) {

					buf[i + 0] = name[i];
					buf[i + 1] = 0;

					if (matching_parameters !=
					    _num_partial_matches(buf, i + 1, command.parameters()))
						break;

					if (matching_arguments !=
					    _num_matching_arguments(buf, i + 1, command))
						break;

					_insert_character(buf[i]);
				}

				/*
				 * If we managed to do a partial completion, let's yield
				 * control to the user.
				 */
				if (i > str_len)
					return;

				/*
				 * No automatic completion was possible, print list of possible
				 * parameters and arguments
				 */
				size_t const pad =
					max(_width_of_partial_matches(str, str_len, command.parameters()),
					    _width_of_matching_arguments(str, str_len, command));

				_list_partial_matches(str, str_len, pad, command.parameters());
				_list_matching_arguments(str, str_len, pad, command);

				_write_newline();
				_fresh_prompt();

				return;
			}

			if (matching_parameters == 1)
				_do_completion(str, str_len, command.parameters());

			if (matching_arguments == 1)
				_do_argument_completion(str, str_len, command);
		}

		void _perform_completion()
		{
			Command *command = _lookup_matching_command();

			if (!command) {
				unsigned const matches = _num_partial_matches(_buf, _cursor_pos, _commands);

				if (matches == 1)
					_do_completion(_buf, _cursor_pos, _commands);

				if (matches > 1) {
					unsigned const pad =
						_width_of_partial_matches(_buf, _cursor_pos, _commands);
					_list_partial_matches(_buf, _cursor_pos, pad, _commands);
					_write_newline();
					_fresh_prompt();
				}
				return;
			}

			/*
			 * We hava a valid command, now try to complete the parameters...
			 */

			/* determine token under the cursor */
			Argument_tracker argument_tracker(*command);

			Token token(_buf, _buf_size);
			for (; token; token = token.next()) {

				argument_tracker.supply_token(token, true);

				if (!argument_tracker.valid())
					return;

				unsigned long const token_pos = (unsigned long)(token.start() - _buf);

				/* we have reached the token under the cursor */
				if (token.type() == Token::IDENT
				 && _cursor_pos >= token_pos
				 && _cursor_pos <= token_pos + token.len()) {

					if (argument_tracker.expect_arg()) {
						char const  *start = token.start();
						size_t const len   = _cursor_pos - token_pos;

						_complete_argument(start, len, *command);
						return;
					}
				}
			}

			/* the cursor is positioned at beginning of new argument */
			if (argument_tracker.expect_arg())
				_complete_argument("", 0, *command);

			if (argument_tracker.expect_space())
				_insert_character(' ');
		}

	public:

		/**
		 * Constructor
		 *
		 * \param prompt    string to be printed at the beginning of the line
		 * \param buf       destination buffer
		 * \param buf_size  destination buffer size
		 * \param terminal  terminal used as output device
		 * \param commands  meta information about commands and their arguments
		 */
		Line_editor(char const *prompt, char *buf, size_t buf_size,
		            Terminal::Session &terminal, Command_registry &commands)
		:
			_prompt(prompt), _prompt_len(strlen(prompt)),
			_buf(buf), _buf_size(buf_size),
			_terminal(terminal), _commands(commands)
		{
			reset();
		}

		/**
		 * Reset prompt to initial state after construction
		 */
		void reset()
		{
			_buf[0] = 0;
			_complete = false;
			_cursor_pos = 0;
			_seq_tracker = Seq_tracker();
			_fresh_prompt();
		}

		/**
		 * Supply a character of user input
		 */
		void submit_input(char c)
		{
			_seq_tracker.input(c);
			_handle_key();
		}

		/**
		 * Returns true if the editing is complete, i.e., the user pressed the
		 * return key.
		 */
		bool completed() const { return _complete; }
};

#endif /* _LINE_EDITOR_H_ */
