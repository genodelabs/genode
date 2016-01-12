/*
 * \brief  Argument list string handling
 * \author Norman Feske
 * \date   2006-05-22
 *
 * Each argument has the form:
 *
 * ! <key>=<value>
 *
 * Key is an identifier that begins with a letter or underline
 * and may also contain digits.
 *
 * A list of arguments is specified by using comma as separator,
 * whereas the first argument is considered as the weakest. If
 * we replace an argument value of an existing argument, the
 * existing argument is removed and we append a new argument at
 * the end of the string.
 */

/*
 * Copyright (C) 2006-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__ARG_STRING_H_
#define _INCLUDE__UTIL__ARG_STRING_H_

#include <util/token.h>
#include <util/string.h>
#include <base/snprintf.h>

namespace Genode {

	class Arg_string;
	class Arg;
}


class Genode::Arg
{
	/**
	 * Define tokenizer used for argument-string parsing
	 *
	 * Argument-string tokens accept C-style identifiers.
	 */
	typedef ::Genode::Token<Scanner_policy_identifier_with_underline> Token;

	friend class Arg_string;

	private:

		Token _key;
		Token _value;

		/**
		 * Return long value of argument
		 *
		 * \param  out_value    argument converted to unsigned long value
		 * \param  out_sign     1 if positive; -1 if negative
		 * \return              true if no syntactic anomaly occured
		 *
		 * This method handles the numberic modifiers G (2^30),
		 * M (2^20), and K (2^10).
		 */
		bool read_ulong(unsigned long *out_value, int *out_sign) const
		{
			Token t = _value;

			/* check for sign; default is positive */
			*out_sign = 1;
			if (t[0] == '+')
				t = t.next();
			else if (t[0] == '-') {
				*out_sign = -1;
				t = t.next();
			}

			/* stop if token after sign is no number */
			if (t.type() != Token::NUMBER)
				return false;

			/* read numeric value and skip the corresponding tokens */
			Number_of_bytes value;
			size_t n = ascii_to(t.start(), value);

			if (n == 0)
				return false;

			t = Token(t.start() + n);
			*out_value = value;

			/* check for strange characters at the end of the number */
			t = t.eat_whitespace();
			if (t && (t[0] != ',')) return false;

			return true;
		}

	public:

		/**
		 * Construct argument from Token(s)
		 */
		Arg(Token t = Token()) : _key(t), _value(0)
		{
			for (; t && (t[0] != ','); t = t.next().eat_whitespace())
				if (t[0] == '=') {
					_value = t.next().eat_whitespace();
					break;
				}
		}

		inline bool valid() const { return _key; }

		unsigned long ulong_value(unsigned long default_value) const
		{
			unsigned long value = 0;
			int sign = 1;

			bool valid = read_ulong(&value, &sign);
			if (sign < 0)
				return default_value;

			return valid ? value : default_value;
		}

		long long_value(long default_value) const
		{
			unsigned long value = 0;
			int sign = 1;

			bool valid = read_ulong(&value, &sign);

			/* FIXME we should check for overflows here! */
			return valid ? sign*value : default_value;
		}

		bool bool_value(bool default_value) const
		{
			bool result = default_value;
			switch(_value.type()) {

			/* result is passed to 'ascii_to' by reference */
			case Token::IDENT:;
				if (ascii_to(_value.start(), result) ==  _value.len())
					return result;

			case Token::STRING:
				if (ascii_to(_value.start()+1, result) == _value.len()-2)
					return result;

			default:
				/* read values 0 (false) / !0 (true) */
				unsigned long value;
				int sign;
				bool valid = read_ulong(&value, &sign);

				return valid ? value : default_value;
			}
		}

		void key(char *dst, size_t dst_len) const
		{
			_key.string(dst, dst_len);
		}

		void string(char *dst, size_t dst_len, const char *default_string) const
		{
			/* check for one-word string w/o quotes */
			if (_value.type() == Token::IDENT) {
				size_t  len = min(dst_len - 1, _value.len());
				memcpy(dst, _value.start(), len);
				dst[len] = 0;
				return;
			}

			/* stop here if _value is not a string */
			if (_value.type() != Token::STRING) {
				strncpy(dst, default_string, dst_len);
				return;
			}

			/* unpack string to dst */
			size_t num_chars = min(dst_len - 1, _value.len());
			unpack_string(_value.start(), dst, num_chars);
		}

		/**
		 * Retrieve a dataspace (page) aligned size argument
		 */
		size_t aligned_size() const
		{
			unsigned long value = ulong_value(0);
			return align_addr(value, 12);
		}
};


class Genode::Arg_string
{
	typedef Arg::Token Token;

	private:

		static Token _next_key(Token t)
		{
			for (; t; t = t.next().eat_whitespace())

				/* if we find a comma, return token after comma */
				if (t[0] == ',') return t.next().eat_whitespace();

			return Token();
		}

		/**
		 * Find key token in argument string
		 */
		static Token _find_key(const char *args, const char *key)
		{
			for (Token t(args); t; t = _next_key(t))

				/* check if key matches */
				if ((t.type() == Token::IDENT) && !strcmp(key, t.start(), t.len()))
					return t;

			return Token();
		}

		/**
		 * Append source string to destination string
		 *
		 * NOTE: check string length before calling this method!
		 *
		 * \return  last character of result string
		 */
		static char *_append(char *dst, const char *src)
		{
			unsigned src_len = strlen(src);
			while (*dst) dst++;
			memcpy(dst, src, src_len + 1);
			return dst + src_len;
		}

	public:

		/**
		 * Find argument by its key
		 */
		static Arg find_arg(const char *args, const char *key) {
			return (args && key) ? Arg(_find_key(args, key)) : Arg(); }

		static Arg first_arg(const char *args) {
			return Arg(Token(args)); }

		/**
		 * Remove argument with the specified key
		 */
		static bool remove_arg(char *args, const char *key)
		{
			if (!args || !key) return false;

			Token beg  = _find_key(args, key);
			Token next = _next_key(beg);

			/* no such key to remove - we are done */
			if (!beg) return true;

			/* if argument is the last one, null-terminate string right here */
			if (!next) {

				/* eat all pending whitespaces at the end of the string */
				char *s = max(beg.start() - 1, args);
				while (s > args && (*s == ' ')) s--;

				/* write string-terminating zero */
				*s = 0;
			} else
				memcpy(beg.start(), next.start(), strlen(next.start()) + 1);

			return true;
		}

		/**
		 * Add new argument
		 */
		static bool add_arg(char *args, unsigned args_len,
		                    const char *key, const char *value,
		                    Token::Type type = Token::Type::IDENT)
		{
			if (!args || !key || !value) return false;

			unsigned old_len = strlen(args);

			/* check if args string has enough capacity */
			if ((type == Token::Type::STRING
			  && old_len + strlen(key) + strlen(value) + 4 > args_len)
			 || (old_len + strlen(key) + strlen(value) + 2 > args_len))
				return false;

			args += old_len;

			if (old_len)
				args = _append(args, ", ");

			if (type == Token::Type::STRING)
				_append(_append(_append(_append(args, key), "=\""), value), "\"");
			else
				_append(_append(_append(args, key), "="), value);
			return true;
		}

		/**
		 * Assign new value to argument
		 */
		static bool set_arg(char *args, unsigned args_len,
		                    const char *key, const char *value)
		{
			return remove_arg(args, key) && add_arg(args, args_len, key, value);
		}

		/**
		 * Assign new integer argument
		 */
		static bool set_arg(char *args, unsigned args_len,
		                    const char *key, int value)
		{
			enum { STRING_LONG_MAX = 32 };
			char buf[STRING_LONG_MAX];
			snprintf(buf, sizeof(buf), "%d", value);
			return remove_arg(args, key) && add_arg(args, args_len, key, buf);
		}

		/**
		 * Assign new string argument
		 */
		static bool set_arg_string(char *args, unsigned args_len,
		                    const char *key, const char *value)
		{
			return remove_arg(args, key)
			    && add_arg(args, args_len, key, value, Token::Type::STRING);
		}
};

#endif /* _INCLUDE__UTIL__ARG_STRING_H_ */
