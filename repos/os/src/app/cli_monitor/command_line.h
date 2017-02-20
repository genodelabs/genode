/*
 * \brief  Utility for command-line parsing
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _COMMAND_LINE_H_
#define _COMMAND_LINE_H_

#include <line_editor.h>

namespace Cli_monitor { class Command_line; }


class Cli_monitor::Command_line
{
	private:

		char const *_cmd_line;
		Command    &_command;

		bool _parameter_is_known(Token token)
		{
			return Argument_tracker::lookup(token, _command.parameters()) != 0;
		}

		Token _tag_token(char const *tag)
		{
			for (Token token(_cmd_line); token; token = token.next())
				if (strcmp(token.start(), tag, token.len()) == 0
				 && strlen(tag) == token.len()
				 && _parameter_is_known(token))
					return token;

			return Token();
		}

		Token _value_token(char const *tag)
		{
			return _tag_token(tag).next().next();
		}

		static bool _is_parameter(Token token)
		{
			return token[0] == '-' && token[1] == '-';
		}


	public:

		/**
		 * Constructor
		 *
		 * \param cmd_line  null-terminated command line string
		 * \param command   meta data about the command
		 */
		Command_line(char const *cmd_line, Command &command)
		: _cmd_line(cmd_line), _command(command) { }

		/**
		 * Return true if tag is specified at the command line
		 */
		bool parameter_exists(char const *tag)
		{
			return _tag_token(tag);
		}

		/**
		 * Return number argument specified for the given tag
		 */
		template <typename T>
		bool parameter(char const *tag, T &result)
		{
			Token value = _value_token(tag);
			return value && Genode::ascii_to(value.start(), result) != 0;
		}

		/**
		 * Return string argument specified for the given tag
		 */
		bool parameter(char const *tag, char *result, size_t result_len)
		{
			Token value = _value_token(tag);
			if (!value)
				return false;

			value.string(result, result_len);
			return true;
		}

		bool argument(unsigned index, char *result, size_t result_len)
		{
			Argument_tracker argument_tracker(_command);

			/* argument counter */
			unsigned cnt = 0;

			for (Token token(_cmd_line); token; token = token.next()) {

				argument_tracker.supply_token(token);

				if (!argument_tracker.valid())
					return false;

				if (!argument_tracker.expect_arg())
					continue;

				Token arg = token.next();
				if (!arg)
					return false;

				/*
				 * The 'arg' token could either the tag of a parameter or
				 * an argument. We only want to count the arguments. So
				 * we skip tokens that have the usual form a parameter tag.
				 */
				if (_is_parameter(arg))
					continue;

				if (cnt == index) {
					arg.string(result, result_len);
					return true;
				}
				cnt++;
			}
			return false;
		}

		/**
		 * Validate parameter tags
		 *
		 * \return tag token of first unexpected parameter, or
		 *         invalid token if no unexpected parameter was found
		 */
		Token unexpected_parameter()
		{
			Argument_tracker argument_tracker(_command);

			for (Token token(_cmd_line); token; token = token.next()) {

				argument_tracker.supply_token(token);

				if (!argument_tracker.valid())
					return token;

				if (!argument_tracker.expect_arg())
					continue;

				Token arg = token.next();

				/* ignore non-parameter tokens (i.e., normal arguments) */
				if (!_is_parameter(arg))
					continue;

				/* if parameter with the given tag exists, we are fine */
				if (_parameter_is_known(arg))
					continue;

				/* we hit an unknown parameter tag */
				return arg;
			}
			return Token();
		}
};

#endif /* _COMMAND_LINE_H_ */
