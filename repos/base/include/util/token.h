/*
 * \brief  Tokenizer support
 * \author Norman Feske
 * \date   2006-05-19
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__TOKEN_H_
#define _INCLUDE__UTIL__TOKEN_H_

#include <util/string.h>

namespace Genode {

	struct Scanner_policy_identifier_with_underline;
	template <typename> class Token;
}


/**
 * Scanner policy that accepts underline characters in identifiers
 */
struct Genode::Scanner_policy_identifier_with_underline
{
	/**
	 * Return true if character belongs to a valid identifier
	 *
	 * \param c  character
	 * \param i  index of character in token
	 * \return   true if character is a valid identifier character
	 *
	 * Letters and underline characters are allowed anywhere in an
	 * identifier, digits must not appear at the beginning.
	 */
	static bool identifier_char(char c, unsigned i) {
		return is_letter(c) || (c == '_') || (i && is_digit(c)); }

	/**
	 * Check for end of quotation
	 *
	 * Checks if next character is non-backslashed quotation mark.
	 * The end of a quoted string is reached when encountering a '"'
	 * character that is not preceded by a backslash.
	 *
	 * \param s  pointer to null-terminated string with at least one
	 *           character
	 */
	static bool end_of_quote(const char *s) {
		return s[0] != '\\' && s[1] == '\"'; }
};


/**
 * Token
 *
 * This class is used to group characters of a string which belong
 * to one syntactical token types number, identifier, string,
 * whitespace or another single character.
 *
 * \param SCANNER_POLICY  policy that defines the way of token scanning
 *
 * See 'Scanner_policy_identifier_with_underline' for an example scanner
 * policy.
 */
template <typename SCANNER_POLICY>
class Genode::Token
{
	public:

		enum Type { SINGLECHAR, NUMBER, IDENT, STRING, WHITESPACE, END };

		/**
		 * Constructor
		 *
		 * \param s        start of string to construct a token from
		 * \param max_len  maximum token length
		 *
		 * The 'max_len' argument is useful for processing character arrays
		 * that are not null-terminated.
		 */
		Token(const char *s = 0, size_t max_len = ~0UL)
		: _start(s), _max_len(max_len), _len(s ? _calc_len(max_len) : 0) { }

		/**
		 * Accessors
		 */
		char *start() const { return (char *)_start; }
		size_t  len() const { return _len; }
		Type   type() const { return _type(_len); }

		/**
		 * Return token as null-terminated string
		 */
		void string(char *dst, size_t max_len) const {
			copy_cstring(dst, start(), min(len() + 1, max_len)); }

		/**
		 * Return true if token is valid
		 */
		bool valid() const { return _start && _len; }

		operator bool () const { return valid(); }

		/**
		 * Access single characters of token
		 */
		char operator [] (int idx) const
		{
			return ((idx >= 0) && ((unsigned)idx < _len)) ? _start[idx] : 0;
		}

		/**
		 * Return next token
		 */
		Token next() const { return Token(_start + _len, _max_len - _len); }

		/**
		 * Return next token after delimiter
		 */
		Token next_after(char const *delim) const
		{
			size_t const len = strlen(delim);

			if (!valid() || len > _max_len)
				return Token();

			char const *s = _start;
			for (size_t rest = _max_len; rest >= len; --rest, ++s)
				if (strcmp(s, delim, len) == 0)
					return Token(s, rest).next();

			return Token();
		}

		/**
		 * Return true if token starts with pattern
		 */
		bool matches(char const *pattern) const
		{
			size_t const len = strlen(pattern);

			if (!valid() || len > _max_len)
				return false;

			return strcmp(pattern, _start, len) == 0;
		}

		/**
		 * Return next non-whitespace token
		 */
		Token eat_whitespace() const { return (_type(_len) == WHITESPACE) ? next() : *this; }

	private:

		const char *_start;
		size_t      _max_len;
		size_t      _len;

		/**
		 * Return type of token
		 *
		 * \param  max_len  maximum token length
		 *
		 * This method is used during the construction of 'Token'
		 * objects, in particular for determining the value of the '_len'
		 * member. Therefore, we explicitely pass the 'max_len' to the
		 * method. For the public interface, there exists the 'type()'
		 * accessor, which relies on '_len' as implicit argument.
		 */
		Type _type(size_t max_len) const
		{
			if (!_start || max_len < 1 || !*_start) return END;

			/* determine the type based on the first character */
			char c = *_start;
			if (SCANNER_POLICY::identifier_char(c, 0)) return IDENT;
			if (is_digit(c))                           return NUMBER;
			if (is_whitespace(c))                      return WHITESPACE;

			/* if string is incomplete, discard it (type END) */
			if (c == '"')
				return _quoted_string_len(max_len) ? STRING : END;

			return SINGLECHAR;
		}

		size_t _quoted_string_len(size_t max_len) const
		{
			/*
			 * The 'end_of_quote' function examines two 'char' values.
			 * Hence, the upper bound of the index is max_len - 2.
			 */
			unsigned i = 0;
			for (; i + 1 < max_len && !SCANNER_POLICY::end_of_quote(&_start[i]); i++)

				/* string ends without final quotation mark? too bad! */
				if (!_start[i]) return 0;

			/* exceeded maximum token length */
			if (i + 1 == max_len)
				return 0;

			/*
			 * We stopped our search at the character before the
			 * final quotation mark but we return the number of
			 * characters including the quotation marks.
			 */
			return i + 2;
		}

		/**
		 * Return length of token
		 */
		size_t _calc_len(size_t max_len) const
		{
			switch (_type(max_len)) {

			case SINGLECHAR:
				return 1;

			case NUMBER:
				{
					unsigned i = 0;
					for (; i < max_len && is_digit(_start[i]); i++);
					return i;
				}

			case IDENT:
				{
					unsigned i = 0;
					for (; i < max_len; i++) {
						if (SCANNER_POLICY::identifier_char(_start[i], i))
							continue;

						/* stop if any other (invalid) character occurs */
						break;
					}
					return i;
				}

			case STRING:

				return _quoted_string_len(max_len);

			case WHITESPACE:
				{
					unsigned i = 0;
					for (; i < max_len && is_whitespace(_start[i]); i++);
					return i;
				}

			case END:
			default:
				return 0;
			}
		}
};

#endif /* _INCLUDE__UTIL__TOKEN_H_ */
