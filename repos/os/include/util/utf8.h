/*
 * \brief  Unicode codepoint type and UTF-8 decoder
 * \author Norman Feske
 * \date   2018-03-15
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__UTIL__UTF8_H_
#define _INCLUDE__OS__UTIL__UTF8_H_

#include <base/stdint.h>

namespace Genode {
	struct Codepoint;
	class  Utf8_ptr;
}


struct Genode::Codepoint
{
	static constexpr uint32_t INVALID = 0xfffd;

	uint32_t value;

	bool valid() const { return value != INVALID; }
};


/**
 * Wrapper around a 'char const' pointer that is able to iterate over UTF-8
 * characters
 *
 * Note that this class is not a smart pointer. It is suffixed with '_ptr' to
 * highlight the fact that it stores a pointer while being copyable. Hence,
 * objects of this type must be handled with the same caution as pointers.
 */
class Genode::Utf8_ptr
{
	private:

		uint8_t const * const _utf8;

		/**
		 * Return true if byte is a tail character of an UTF-8 sequence
		 */
		static bool _tail_char(uint8_t c) { return (c & 0xc0) == 0x80; }

		/**
		 * Return expected number of bytes following the 'c1' start of an
		 * UTF-8 sequence
		 */
		static unsigned _tail_length(uint8_t c1)
		{
			if (c1 < 128)
				return 0;

			/* bit 7 is known to be set, count the next set bits */
			for (unsigned i = 0; i < 4; i++)
				if ((c1 & (1 << (6 - i))) == 0)
					return i;

			return 0;
		}

		/**
		 * Consume trailing bytes of UTF-8 sequence of length 'n'
		 *
		 * \param c1  character bits of the initial UTF-8 byte
		 */
		static Codepoint _decode_tail(uint32_t c1, uint8_t const *utf8, unsigned n)
		{
			uint32_t value = c1;

			for (unsigned i = 0; i < n; i++, utf8++) {

				/* detect premature end of string or end of UTF-8 sequence */
				uint8_t const c = *utf8;
				if (!c || !_tail_char(c))
					return Codepoint { Codepoint::INVALID };

				value = (value << 6) | (c & 0x3f);
			}

			/* reject overlong sequences */
			bool const overlong = ((n > 0 && value <    0x80)
			                    || (n > 1 && value <   0x800)
			                    || (n > 2 && value < 0x10000));

			/* conflict with UTF-16 surrogate halves or reserved codepoints */
			bool const illegal = (n > 1) && ((value >= 0xd800 && value <= 0xdfff)
			                              || (value >= 0xfdd0 && value <= 0xfdef)
			                              || (value == 0xfffe)
			                              || (value >  0x10ffff));

			bool const valid = !overlong && !illegal;

			return Codepoint { valid ? value : Codepoint::INVALID };
		}

		bool _end() const { return !_utf8 || !*_utf8; }

		/**
		 * Scan for the null termination of '_utf8'
		 *
		 * \param max  maximum number of bytes to scan
		 * \return     number of present bytes, up to 'max'
		 */
		unsigned _bytes_present(unsigned max) const
		{
			for (unsigned i = 0; i < max; i++)
				if (!_utf8[i])
					return i;

			return max;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param utf8  null-terminated buffer containing UTF-8-encoded text
		 */
		Utf8_ptr(char const *utf8) : _utf8((uint8_t const *)utf8) { }

		Utf8_ptr(Utf8_ptr const &other) : _utf8(other._utf8) { }

		Utf8_ptr &operator = (Utf8_ptr const &other)
		{
			const_cast<uint8_t const *&>(_utf8) = other._utf8;
			return *this;
		}

		/**
		 * Return next UTF-8 character
		 */
		Utf8_ptr const next() const
		{
			if (_end()) return Utf8_ptr(nullptr);

			unsigned        const tail_length = _tail_length(_utf8[0]);
			uint8_t const * const tail        = _utf8 + 1;

			for (unsigned i = 0; i < tail_length; i++)
				if (!_tail_char(tail[i]))
					return Utf8_ptr((char const *)tail + i);

			return Utf8_ptr((char const *)tail + tail_length);
		}

		/**
		 * Return true if string contains a complete UTF-8 sequence
		 *
		 * This method solely checks for a premature truncation of the string.
		 * It does not check the validity of the UTF-8 sequence. The success of
		 * 'complete' method is a precondition for the correct operation of the
		 * 'next' or 'codepoint' methods. A complete sequence may still yield
		 * an invalid 'Codepoint'.
		 */
		bool complete() const
		{
			if (_end()) return false;

			unsigned const expected_length = _tail_length(_utf8[0]) + 1;

			return expected_length == _bytes_present(expected_length);
		}

		/**
		 * Return character as Unicode codepoint
		 */
		Codepoint codepoint() const
		{
			uint8_t const *s = _utf8;
			uint8_t const c1 = *s++;

			if ((c1 & 0x80) == 0)    return Codepoint { c1 };
			if ((c1 & 0xe0) == 0xc0) return _decode_tail(c1 & 0x1f, s, 1);
			if ((c1 & 0xf0) == 0xe0) return _decode_tail(c1 & 0x0f, s, 2);
			if ((c1 & 0xf8) == 0xf0) return _decode_tail(c1 & 0x07, s, 3);

			return Codepoint { Codepoint::INVALID };
		}

		/**
		 * Return length of UTF-8 sequence in bytes
		 */
		unsigned length() const
		{
			return _end() ? 0 : _bytes_present(1 + _tail_length(_utf8[0]));
		}
};

#endif /* _INCLUDE__OS__UTIL__UTF8_H_ */
