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
#include <util/meta.h>
#include <util/attempt.h>
#include <util/noncopyable.h>
#include <cpu/string.h>

namespace Genode {

	struct Num_bytes;
	struct Byte_range_ptr;
	class Span;
	class Cstring;
	template <Genode::size_t> class String;

	using Const_byte_range_ptr = Span;
	using Number_of_bytes = Num_bytes; /* deprecated */

	int memcmp(void const *, void const *, size_t);
	void *memcpy(void *, const void *, size_t);
}


/**
 * Data structure for describing a mutable byte buffer
 *
 * The type is intended to be used as 'Byte_range_ptr const &' argument.
 */
struct Genode::Byte_range_ptr : Noncopyable
{
	struct {
		char * const start;
		size_t const num_bytes;
	};

	Byte_range_ptr(char *start, size_t num_bytes)
	: start(start), num_bytes(num_bytes) { }

	void with_skipped_bytes(size_t const n, auto const &fn)
	{
		if (num_bytes <= n)
			return;

		Byte_range_ptr const remainder { start + n, num_bytes - n };
		fn(remainder);
	}

	/**
	 * Call 'fn' with an 'Output' interface for printing into the byte range
	 *
	 * \return number of printed bytes
	 */
	Attempt<size_t, Buffer_error> as_output(auto const &fn) const
	{
		struct _Output : Output
		{
			Byte_range_ptr const _bytes;

			size_t used = 0;
			bool   exceeded = false;

			_Output(Byte_range_ptr const &b) : _bytes(b.start, b.num_bytes) { }

			[[nodiscard]] bool _fits(size_t const n)
			{
				if (used + n > _bytes.num_bytes) exceeded = true;
				return !exceeded;
			}

			void out_char(char c) override
			{
				if (_fits(1)) _bytes.start[used++] = c;
			}

			void out_string(char const *str, size_t n) override
			{
				if (_fits(n)) {
					Genode::memcpy(_bytes.start + used, str, n);
					used += n;
				}
			}
		} byte_range_output { *this };

		Output &out = byte_range_output;
		fn(out);

		if (byte_range_output.exceeded)
			return Buffer_error::EXCEEDED;

		return byte_range_output.used;
	}
};


/**
 * Data structure for describing a constant byte buffer
 */
struct Genode::Span : Noncopyable
{
	struct {
		char const * const start;
		size_t       const num_bytes;
	};

	bool contains(char const *ptr) const
	{
		return (ptr >= start) && size_t(ptr - start) < num_bytes;
	}

	bool contains(void const *ptr) const
	{
		return contains((char const *)ptr);
	}

	bool equals(Span const &other) const
	{
		return num_bytes == other.num_bytes
		    && !memcmp(other.start, start, num_bytes);
	}

	bool starts_with(Span const &prefix) const
	{
		return num_bytes >= prefix.num_bytes
		    && !memcmp(start, prefix.start, prefix.num_bytes);
	}

	bool ends_with(Span const &suffix) const
	{
		return num_bytes >= suffix.num_bytes
		    && !memcmp(start + num_bytes - suffix.num_bytes,
		               suffix.start, suffix.num_bytes);
	}

	/**
	 * Call 'fn' with two spans preceeding and following the 'match' character
	 */
	void cut(char const match, auto const &fn) const
	{
		unsigned n = 0;
		while (n < num_bytes && (start[n] != match))
			n++;

		if (n < num_bytes)
			fn(Span(start, n), Span(start + n + 1, num_bytes - n - 1));
		else
			fn(*this, Span(nullptr, 0));
	}

	/**
	 * Call 'fn' for each part of the 'Span' separated by 'sep'
	 */
	void split(char sep, auto const &fn) const
	{
		char const *s = start;
		size_t      n = num_bytes;

		while (n)
			Span(s, n).cut(sep, [&] (Span const &head, Span const &tail) {
				fn(head);
				s = tail.start;
				n = tail.num_bytes; });

		/* handle special case of a trailing separator */
		if (num_bytes > 0 && start[num_bytes - 1] == sep) fn(Span { nullptr, 0u });
	}

	/**
	 * Call 'fn' with the part of the span excluding leading/trailing spaces
	 */
	template <typename FN>
	auto trimmed(FN const &fn) const
	-> typename Trait::Functor<decltype(&FN::operator())>::Return_type
	{
		auto space = [] (char c) { return c == ' '; };

		char const *s = start; size_t n = num_bytes;
		for (; n && space(s[0]); s++, n--);
		for (; n && space(s[n - 1]); n--);

		return fn(Span(s, n));
	}

	Span(char const *start, size_t num_bytes) : start(start), num_bytes(num_bytes) { }
};


/***********************
 ** Utility functions **
 ***********************/

namespace Genode {

	/**
	 * Return length of null-terminated string in bytes
	 */
	__attribute((optimize("no-tree-loop-distribute-patterns")))
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
		if (!size)
			return dst;

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
	 *
	 * In contrast to the POSIX 'strncpy' function, 'copy_cstring' always
	 * produces a null-terminated string in the 'dst' buffer if the 'size'
	 * argument is greater than 0.
	 */
	inline void copy_cstring(char *dst, const char *src, size_t size)
	{
		/* sanity check for corner case of a zero-size destination buffer */
		if (size == 0) return;

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
	 * Clear byte buffer
	 *
	 * \param dst   destination buffer
	 * \param size  buffer size in bytes
	 *
	 * The compiler attribute is needed to prevent the generation of a
	 * 'bzero()' call in the 'while' loop with gcc 10.
	 */
	__attribute((optimize("no-tree-loop-distribute-patterns")))
	inline void bzero(void *dst, size_t size)
	{
		using word_t = unsigned long;

		static constexpr size_t LEN = sizeof(word_t), MASK = LEN - 1;

		size_t d_align = (size_t)dst & MASK;
		uint8_t *d = (uint8_t*)dst;

		/* write until word aligned */
		for (; d_align && d_align < LEN && size; d_align++, size--, d++)
			*d = 0;

		/* write 8-word chunks (likely matches cache line size) */
		for (; size >= 8*LEN; size -= 8*LEN, d += 8*LEN) {
			((word_t *)d)[0] = 0; ((word_t *)d)[1] = 0;
			((word_t *)d)[2] = 0; ((word_t *)d)[3] = 0;
			((word_t *)d)[4] = 0; ((word_t *)d)[5] = 0;
			((word_t *)d)[6] = 0; ((word_t *)d)[7] = 0;
		}

		/* write remaining words */
		for (; size >= LEN; size -= LEN, d += LEN)
			((word_t *)d)[0] = 0;

		/* write remaining bytes */
		for (; size; size--, d++)
			*d = 0;

		/* prevent the compiler from dropping bzero */
		asm volatile(""::"r"(dst):"memory");
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
	 * Read unsigned long value from span
	 *
	 * \param s       text buffer
	 * \param result  destination variable
	 * \param base    integer base
	 * \return        number of consumed characters
	 *
	 * If the base argument is 0, the integer base is detected based on the
	 * characters in front of the number. If the number is prefixed with "0x",
	 * a base of 16 is used, otherwise a base of 10.
	 */
	template <typename T>
	inline size_t parse_unsigned(Span const &s, T &result, uint8_t base = 0)
	{
		if (!s.num_bytes) return 0;

		size_t i = 0;

		/* autodetect hexadecimal base, default is a base of 10 */
		if (base == 0 && s.num_bytes > 2) {

			/* read '0x' prefix */
			if (s.start[0] == '0' && (s.start[1] == 'x' || s.start[1] == 'X')) {
				i += 2;
				base = 16;
			}
		}
		if (base == 0) base = 10;

		/* read number */
		T value = 0;
		for (; i < s.num_bytes; i++) {

			/* read digit, stop when hitting a non-digit character */
			int const d = digit(s.start[i], base == 16);
			if (d < 0)
				break;

			/* append digit to integer value */
			uint8_t const valid_digit = (uint8_t)d;
			value = (T)(value*base + valid_digit);
		}

		result = value;
		return i;
	}


	/**
	 * Read signed value from span
	 *
	 * \return number of consumed characters
	 */
	template <typename T>
	inline size_t parse_signed(Span const &s, T &result)
	{
		if (!s.num_bytes) return 0;

		/* read sign */
		int    const sign = (s.start[0] == '-') ? -1 : 1;
		size_t const i    = (s.start[0] == '-' || s.start[0] == '+') ? 1 : 0;

		T value = 0;
		size_t const j = parse_unsigned({ s.start + i, s.num_bytes - i }, value, 0);

		if (!j)
			return 0;

		result = sign*value;
		return i + j;
	}


	/**
	 * Read boolean value from span
	 *
	 * \return number of consumed characters
	 */
	inline size_t parse(Span const &s, bool &result)
	{
		struct Pattern { char const *text; size_t len; bool value; };
		static Pattern const patterns[6] { { "yes",  3, true }, { "no",    2, false },
		                                   { "true", 4, true }, { "false", 5, false },
		                                   { "on",   2, true }, { "off",   3, false } };
		for (Pattern const &p : patterns)
			if (p.len <= s.num_bytes && !memcmp(s.start, p.text, p.len)) {
				result = p.value;
				return p.len; }
		return 0;
	}


	inline size_t parse(Span const &s, unsigned char      &out) { return parse_unsigned(s, out); }
	inline size_t parse(Span const &s, unsigned short     &out) { return parse_unsigned(s, out); }
	inline size_t parse(Span const &s, unsigned long      &out) { return parse_unsigned(s, out); }
	inline size_t parse(Span const &s, unsigned long long &out) { return parse_unsigned(s, out); }
	inline size_t parse(Span const &s, unsigned int       &out) { return parse_unsigned(s, out); }
	inline size_t parse(Span const &s, long               &out) { return parse_signed(s, out); }
	inline size_t parse(Span const &s, int                &out) { return parse_signed(s, out); }


	/**
	 * Read double float value from span
	 *
	 * \return number of consumed characters
	 */
	inline size_t parse(Span const &s, double &result)
	{
		double v = 0.0;    /* decimal part              */
		double d = 0.1;    /* power of fractional digit */
		bool neg = false;  /* sign                      */
		size_t i = 0;      /* character counter         */

		if (!s.num_bytes) return 0;

		if (s.start[0] == '-') {
			neg = true;
			i++;
		}

		/* parse decimal part of number */
		for (; i < s.num_bytes && s.start[i] && is_digit(s.start[i]); i++)
			v = 10*v + digit(s.start[i], false);

		result = v;

		/* skip comma */
		if (i < s.num_bytes && s.start[i] == '.')
			i++;

		/* parse fractional part of number */
		for (; i < s.num_bytes && s.start[i] && is_digit(s.start[i]); i++, d *= 0.1)
			v += d*digit(s.start[i], false);

		result = neg ? -v : v;
		return i;
	}


	/**
	 * Assign parsed information to object 'obj'
	 *
	 * The object type must provide a 'parse(Span &)' method that applies
	 * the interpreted text 's' to the object.
	 */
	static inline size_t parse(Span const &s, auto &obj) { return obj.parse(s); }


	/**
	 * Unpack quoted string
	 *
	 * \param src   source string including the quotation marks ("...")
	 * \param dst   destination buffer
	 *
	 * \return      number of characters or ~0UL on error
	 */
	inline size_t unpack_string(const char *src, char *dst, size_t const dst_len)
	{
		/* check if quoted string */
		if ((*src != '"') || (dst_len < 1))
			return ~0UL;

		src++;

		auto end_of_quote = [] (const char *s) {
			return s[0] != '\\' && s[1] == '\"'; };

		size_t i = 0;
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


	/**
	 * API compatibility helper
	 *
	 * \noapi
	 * \deprecated
	 */
	inline size_t ascii_to_unsigned(const char *s, auto &out, uint8_t base)
	{
		return parse_unsigned(Span { s, ~0UL }, out, base);
	}


	/**
	 * API compatibility helper
	 *
	 * \noapi
	 * \deprecated
	 */
	inline size_t ascii_to(const char *s, auto &out)
	{
		return parse(Span { s, ~0UL }, out);
	}
}


/**
 * Helper for parsing and printing memory-sized amounts of bytes
 */
struct Genode::Num_bytes
{
	size_t _n;

	operator size_t() const { return _n; }

	static void print(Output &output, auto const n)
	{
		using Genode::print;

		enum { KB = 1024U, MB = KB*1024U, GB = MB*1024U };

		if      (n      == 0) print(output, 0);
		else if (n % GB == 0) print(output, n/GB, "G");
		else if (n % MB == 0) print(output, n/MB, "M");
		else if (n % KB == 0) print(output, n/KB, "K");
		else                  print(output, n);
	}

	void print(Output &output) const { print(output, _n); }

	/**
	 * Read 'Num_bytes' value from span and handle the size suffixes
	 *
	 * This function scales the resulting size value according to the suffixes
	 * for G (2^30), M (2^20), and K (2^10) if present.
	 *
	 * \return number of consumed characters
	 */
	static size_t parse(Span const &s, auto &out)
	{
		unsigned long res = 0;

		/* convert numeric part */
		size_t i = parse_unsigned(s, res, 0);

		/* handle suffixes */
		if (i > 0 && i < s.num_bytes)
			switch (s.start[i]) {
			case 'G': res *= 1024*1024*1024; i++; break;
			case 'M': res *= 1024*1024;      i++; break;
			case 'K': res *= 1024;           i++; break;
			default: break;
			}

		if (i) out = res;
		return i;
	}

	size_t parse(Span const &s) { return parse(s, _n); }
};


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
			for (;  res < max_len && str && *str; str++, res++);
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
		template <typename T>
		String(T const &head, auto &&... tail)
		{
			/* initialize string content */
			Local_output output(_buf);
			Genode::print(output, head, tail...);

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
			copy_cstring(_buf, cstr, _len);
		}

		/**
		 * Copy constructor
		 */
		template <unsigned N>
		String(String<N> const &other) : _len(min(other.length(), CAPACITY))
		{
			copy_cstring(_buf, other.string(), _len);
		}

		/**
		 * Return length of string, including the terminating null character
		 */
		size_t length() const { return _len; }

		static constexpr size_t capacity() { return CAPACITY; }

		bool valid() const {
			return (_len <= CAPACITY) && (_len != 0) && (_buf[_len - 1] == '\0'); }

		char const *string() const { return valid() ? _buf : ""; }

		template <typename FN>
		auto with_span(FN const &fn) const
		-> typename Trait::Functor<decltype(&FN::operator())>::Return_type
		{
			return fn(Span { _buf, max(1ul, _len) - 1 });
		}

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

		template <size_t N>
		bool operator > (String<N> const &other) const
		{
			return strcmp(string(), other.string()) > 0;
		}

		void print(Output &out) const { Genode::print(out, string()); }
};

#endif /* _INCLUDE__UTIL__STRING_H_ */
