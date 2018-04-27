/*
 * \brief  String utilities
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__STRING_H_
#define _INCLUDE__UTIL__STRING_H_

#include <base/stdint.h>
#include <base/output.h>
#include <util/misc_math.h>
#include <cpu/string.h>

namespace Genode {

	class Number_of_bytes;
	class Cstring;
	template <Genode::size_t> class String;
}


/**
 * Wrapper of 'size_t' for selecting the 'ascii_to' function to parse byte values
 */
class Genode::Number_of_bytes
{
	size_t _n;

	public:

		/**
		 * Default constructor
		 */
		Number_of_bytes() : _n(0) { }

		/**
		 * Constructor, to be used implicitly via assignment operator
		 */
		Number_of_bytes(size_t n) : _n(n) { }

		/**
		 * Convert number of bytes to 'size_t' value
		 */
		operator size_t() const { return _n; }

		void print(Output &output) const
		{
			using Genode::print;

			enum { KB = 1024UL, MB = KB*1024UL, GB = MB*1024UL };

			if      (_n      == 0) print(output, 0);
			else if (_n % GB == 0) print(output, _n/GB, "G");
			else if (_n % MB == 0) print(output, _n/MB, "M");
			else if (_n % KB == 0) print(output, _n/KB, "K");
			else                   print(output, _n);
		}
};


/***********************
 ** Utility functions **
 ***********************/

namespace Genode {

	/**
	 * Return length of null-terminated string in bytes
	 */
	inline size_t strlen(const char *s)
	{
		size_t res = 0;
		for (; s && *s; s++, res++);
		return res;
	}


	/**
	 * Compare two strings
	 *
	 * \param len   maximum number of characters to compare,
	 *              default is unlimited
	 *
	 * \return   0 if both strings are equal, or
	 *           a positive number if s1 is higher than s2, or
	 *           a negative number if s1 is lower than s2
	 */
	inline int strcmp(const char *s1, const char *s2, size_t len = ~0UL)
	{
		for (; *s1 && *s1 == *s2 && len; s1++, s2++, len--) ;
		return len ? *s1 - *s2 : 0;
	}


	/**
	 * Copy memory buffer to a potentially overlapping destination buffer
	 *
	 * \param dst   destination memory block
	 * \param src   source memory block
	 * \param size  number of bytes to move
	 *
	 * \return      pointer to destination memory block
	 */
	inline void *memmove(void *dst, const void *src, size_t size)
	{
		char *d = (char *)dst, *s = (char *)src;
		size_t i;

		if (s > d)
			for (i = 0; i < size; i++, *d++ = *s++);
		else
			for (s += size - 1, d += size - 1, i = size; i-- > 0; *d-- = *s--);

		return dst;
	}


	/**
	 * Copy memory buffer to a non-overlapping destination buffer
	 *
	 * \param dst   destination memory block
	 * \param src   source memory block
	 * \param size  number of bytes to copy
	 *
	 * \return      pointer to destination memory block
	 */
	inline void *memcpy(void *dst, const void *src, size_t size)
	{
		char *d = (char *)dst, *s = (char *)src;
		size_t i;

		/* check for overlap */
		if ((d + size > s) && (s + size > d))
			return memmove(dst, src, size);

		/* try cpu specific version first */
		if ((i = size - memcpy_cpu(dst, src, size)) == size)
			return dst;

		d += i; s += i; size -= i;

		/* copy eight byte chunks */
		for (i = size >> 3; i > 0; i--, *d++ = *s++,
		                                *d++ = *s++,
		                                *d++ = *s++,
		                                *d++ = *s++,
		                                *d++ = *s++,
		                                *d++ = *s++,
		                                *d++ = *s++,
		                                *d++ = *s++);

		/* copy left over */
		for (i = 0; i < (size & 0x7); i++, *d++ = *s++);

		return dst;
	}


	/**
	 * Copy string
	 *
	 * \param dst   destination buffer
	 * \param src   buffer holding the null-terminated source string
	 * \param size  maximum number of characters to copy
	 * \return      pointer to destination string
	 *
	 * Note that this function is not fully compatible to the C standard, in
	 * particular there is no zero-padding if the length of 'src' is smaller
	 * than 'size'. Furthermore, in contrast to the libc version, this function
	 * always produces a null-terminated string in the 'dst' buffer if the
	 * 'size' argument is greater than 0.
	 */
	inline char *strncpy(char *dst, const char *src, size_t size)
	{
		/* sanity check for corner case of a zero-size destination buffer */
		if (size == 0) return dst;

		/* backup original 'dst' for the use as return value */
		char *orig_dst = dst;

		/*
		 * Copy characters from 'src' to 'dst' respecting the 'size' limit.
		 * In each iteration, the 'size' variable holds the maximum remaining
		 * size. We have to leave at least one character free to add the null
		 * termination afterwards.
		 */
		while ((size-- > 1UL) && *src)
			*dst++ = *src++;

		/* append null termination to the destination buffer */
		*dst = 0;

		return orig_dst;
	}


	/**
	 * Compare memory blocks
	 *
	 * \return  0 if both memory blocks are equal, or
	 *          a negative number if 'p0' is less than 'p1', or
	 *          a positive number if 'p0' is greater than 'p1'
	 */
	inline int memcmp(const void *p0, const void *p1, size_t size)
	{
		const unsigned char *c0 = (const unsigned char *)p0;
		const unsigned char *c1 = (const unsigned char *)p1;

		size_t i;
		for (i = 0; i < size; i++)
			if (c0[i] != c1[i]) return c0[i] - c1[i];

		return 0;
	}


	/**
	 * Fill destination buffer with given value
	 *
	 * \param dst   destination buffer
	 * \param i     byte value
	 * \param size  buffer size in bytes
	 */
	inline void *memset(void *dst, int i, size_t size)
	{
		while (size--) ((char *)dst)[size] = i;
		return dst;
	}


	/**
	 * Convert ASCII character to digit
	 *
	 * \param hex   consider hexadecimals
	 * \return      digit or -1 on error
	 */
	inline int digit(char c, bool hex = false)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (hex && c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (hex && c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	}


	/**
	 * Return true if character is a letter
	 */
	inline bool is_letter(char c)
	{
		return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
	}


	/**
	 * Return true if character is a digit
	 */
	inline bool is_digit(char c, bool hex = false)
	{
		return (digit(c, hex) >= 0);
	}


	/**
	 * Return true if character is whitespace
	 */
	inline bool is_whitespace(char c)
	{
		return (c == '\t' || c == ' ' || c == '\n');
	}


	/**
	 * Read unsigned long value from string
	 *
	 * \param s       source string
	 * \param result  destination variable
	 * \param base    integer base
	 * \return        number of consumed characters
	 *
	 * If the base argument is 0, the integer base is detected based on the
	 * characters in front of the number. If the number is prefixed with "0x",
	 * a base of 16 is used, otherwise a base of 10.
	 */
	template <typename T>
	inline size_t ascii_to_unsigned(const char *s, T &result, unsigned base)
	{
		T i = 0, value = 0;

		if (!*s) return i;

		/* autodetect hexadecimal base, default is a base of 10 */
		if (base == 0) {

			/* read '0x' prefix */
			if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {
				s += 2; i += 2;
				base = 16;
			} else
				base = 10;
		}

		/* read number */
		for (int d; ; s++, i++) {

			/* read digit, stop when hitting a non-digit character */
			if ((d = digit(*s, base == 16)) < 0) break;

			/* append digit to integer value */
			value = value*base + d;
		}

		result = value;
		return i;
	}


	/**
	 * Read boolean value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(char const *s, bool &result)
	{
		if (!strcmp(s, "yes",   3)) { result = true;  return 3; }
		if (!strcmp(s, "true",  4)) { result = true;  return 4; }
		if (!strcmp(s, "on",    2)) { result = true;  return 2; }
		if (!strcmp(s, "no",    2)) { result = false; return 2; }
		if (!strcmp(s, "false", 5)) { result = false; return 5; }
		if (!strcmp(s, "off",   3)) { result = false; return 3; }

		return 0;
	}


	/**
	 * Read unsigned char value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, unsigned char &result)
	{
		return ascii_to_unsigned(s, result, 0);
	}


	/**
	 * Read unsigned short value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, unsigned short &result)
	{
		return ascii_to_unsigned(s, result, 0);
	}


	/**
	 * Read unsigned long value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, unsigned long &result)
	{
		return ascii_to_unsigned(s, result, 0);
	}


	/**
	 * Read unsigned long long value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, unsigned long long &result)
	{
		return ascii_to_unsigned(s, result, 0);
	}



	/**
	 * Read unsigned int value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, unsigned int &result)
	{
		return ascii_to_unsigned(s, result, 0);
	}


	/**
	 * Read signed long value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, long &result)
	{
		int i = 0;

		/* read sign */
		int sign = (*s == '-') ? -1 : 1;

		if (*s == '-' || *s == '+') { s++; i++; }

		int j = 0;
		unsigned long value = 0;

		j = ascii_to_unsigned(s, value, 10);

		if (!j) return i;

		result = sign*value;
		return i + j;
	}


	/**
	 * Read 'Number_of_bytes' value from string and handle the size suffixes
	 *
	 * This function scales the resulting size value according to the suffixes
	 * for G (2^30), M (2^20), and K (2^10) if present.
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, Number_of_bytes &result)
	{
		unsigned long res = 0;

		/* convert numeric part of string */
		int i = ascii_to_unsigned(s, res, 0);

		/* handle suffixes */
		if (i > 0)
			switch (s[i]) {
			case 'G': res *= 1024;
			case 'M': res *= 1024;
			case 'K': res *= 1024; i++;
			default: break;
			}

		result = res;
		return i;
	}


	/**
	 * Read double float value from string
	 *
	 * \return number of consumed characters
	 */
	inline size_t ascii_to(const char *s, double &result)
	{
		double v = 0.0;    /* decimal part              */
		double d = 0.1;    /* power of fractional digit */
		bool neg = false;  /* sign                      */
		int    i = 0;      /* character counter         */

		if (s[i] == '-') {
			neg = true;
			i++;
		}

		/* parse decimal part of number */
		for (; s[i] && is_digit(s[i]); i++)
			v = 10*v + digit(s[i], false);

		/* if no fractional part exists, return current value */
		if (s[i] != '.') {
			result = neg ? -v : v;
			return i;
		}

		/* skip comma */
		i++;

		/* parse fractional part of number */
		for (; s[i] && is_digit(s[i]); i++, d *= 0.1)
			v += d*digit(s[i], false);

		result = neg ? -v : v;
		return i;
	}


	/**
	 * Check for end of quotation
	 *
	 * Checks if next character is non-backslashed quotation mark.
	 */
	inline bool end_of_quote(const char *s) {
		return s[0] != '\\' && s[1] == '\"'; }


	/**
	 * Unpack quoted string
	 *
	 * \param src   source string including the quotation marks ("...")
	 * \param dst   destination buffer
	 *
	 * \return      number of characters or negative error code
	 */
	inline int unpack_string(const char *src, char *dst, int dst_len)
	{
		/* check if quoted string */
		if (*src != '"') return -1;

		src++;

		int i = 0;
		for (; *src && !end_of_quote(src - 1) && (i < dst_len - 1); i++) {

			/* transform '\"' to '"' */
			if (src[0] == '\\' && src[1] == '\"') {
				*dst++ = '"';
				src += 2;
			} else
				*dst++ = *src++;
		}

		/* write terminating null */
		*dst = 0;

		return i;
	}
}


/**
 * Helper for the formatted output of a length-constrained character buffer
 */
class Genode::Cstring
{
	private:

		char const * const _str;
		size_t       const _len;

		static size_t _init_len(char const *str, size_t max_len)
		{
			/*
			 * In contrast to 'strlen' we stop searching for a terminating
			 * null once we reach 'max_len'.
			 */
			size_t res = 0;
			for (; str && *str && res < max_len; str++, res++);
			return res;
		}

	public:

		/**
		 * Constructor
		 *
		 * \param str  null-terminated character buffer
		 */
		Cstring(char const *str) : _str(str), _len(strlen(str)) { }

		/**
		 * Constructor
		 *
		 * \param str      character buffer, not neccessarily null-terminated
		 * \param max_len  maximum number of characters to consume
		 *
		 * The 'Cstring' contains all characters up to a terminating null in
		 * the 'str' buffer but not more that 'max_len' characters.
		 */
		Cstring(char const *str, size_t max_len)
		:
			_str(str), _len(_init_len(str, max_len))
		{ }

		void print(Output &out) const { out.out_string(_str, _len); }

		size_t length() const { return _len; }
};


/**
 * Buffer that contains a null-terminated string
 *
 * \param CAPACITY  buffer size including the terminating zero,
 *                  must be higher than zero
 */
template <Genode::size_t CAPACITY>
class Genode::String
{
	private:

		char _buf[CAPACITY];

		/**
		 * Number of chars contained in '_buf' including the terminating null
		 */
		size_t _len { 0 };

		/**
		 * Output facility that targets a character buffer
		 */
		class Local_output : public Output
		{
			private:

				/*
				 * Noncopyable
				 */
				Local_output(Local_output const &);
				Local_output &operator = (Local_output const &);

			public:

				char * const _buf;

				size_t _num_chars = 0;

				/**
				 * Return true if '_buf' can fit at least one additional 'char'.
				 */
				bool _capacity_left() const { return CAPACITY - _num_chars - 1; }

				void _append(char c) { _buf[_num_chars++] = c; }

				Local_output(char *buf) : _buf(buf) { }

				size_t num_chars() const { return _num_chars; }

				void out_char(char c) override { if (_capacity_left()) _append(c); }

				void out_string(char const *str, size_t n) override
				{
					while (n-- > 0 && _capacity_left() && *str)
						_append(*str++);
				}
		};

	public:

		constexpr static size_t size() { return CAPACITY; }

		String() { }

		/**
		 * Constructor
		 *
		 * The constructor accepts a non-zero number of arguments, which
		 * are concatenated in the resulting 'String' object. In order to
		 * generate the textual representation of the arguments, the
		 * argument types must support the 'Output' interface, e.g., by
		 * providing 'print' method.
		 *
		 * If the textual representation of the supplied arguments exceeds
		 * 'CAPACITY', the resulting string gets truncated. The caller may
		 * check for this condition by evaluating the 'length' of the
		 * constructed 'String'. If 'length' equals 'CAPACITY', the string
		 * may fit perfectly into the buffer or may have been truncated.
		 * In general, it would be safe to assume the latter.
		 */
		template <typename T, typename... TAIL>
		String(T const &arg, TAIL &&... args)
		{
			/* initialize string content */
			Local_output output(_buf);
			Genode::print(output, arg, args...);

			/* add terminating null */
			_buf[output.num_chars()] = 0;
			_len = output.num_chars() + 1;
		}

		/**
		 * Constructor
		 *
		 * Overload for the common case of constructing a 'String' from a
		 * string literal.
		 */
		String(char const *cstr) : _len(min(Genode::strlen(cstr) + 1, CAPACITY))
		{
			Genode::strncpy(_buf, cstr, _len);
		}

		/**
		 * Copy constructor
		 */
		template <unsigned N>
		String(String<N> const &other) : _len(min(other.length(), CAPACITY))
		{
			Genode::strncpy(_buf, other.string(), _len);
		}

		/**
		 * Return length of string, including the terminating null character
		 */
		size_t length() const { return _len; }

		static constexpr size_t capacity() { return CAPACITY; }

		bool valid() const {
			return (_len <= CAPACITY) && (_len != 0) && (_buf[_len - 1] == '\0'); }

		char const *string() const { return valid() ? _buf : ""; }

		bool operator == (char const *other) const
		{
			return strcmp(string(), other) == 0;
		}

		bool operator != (char const *other) const
		{
			return strcmp(string(), other) != 0;
		}

		template <size_t OTHER_CAPACITY>
		bool operator == (String<OTHER_CAPACITY> const &other) const
		{
			return strcmp(string(), other.string()) == 0;
		}

		template <size_t OTHER_CAPACITY>
		bool operator != (String<OTHER_CAPACITY> const &other) const
		{
			return strcmp(string(), other.string()) != 0;
		}

		void print(Output &out) const { Genode::print(out, string()); }
};

#endif /* _INCLUDE__UTIL__STRING_H_ */
