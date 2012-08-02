/*
 * \brief  String utility functions
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2006-05-10
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__STRING_H_
#define _INCLUDE__UTIL__STRING_H_

#include <base/stdint.h>
#include <util/misc_math.h>
#include <cpu/string.h>

namespace Genode {

	/**
	 * Determine length of null-terminated string
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
	 * \retval   0  strings are equal
	 * \retval  >0  s1 is higher than s2
	 * \retval  <0  s1 is lower than s2
	 */
	inline int strcmp(const char *s1, const char *s2, size_t len = ~0UL)
	{
		for (; *s1 && *s1 == *s2 && len; s1++, s2++, len--) ;
		return len ? *s1 - *s2 : 0;
	}


	/**
	 * Copy memory block
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
	 * Memmove wrapper for sophisticated overlapping-aware memcpy
	 */
	inline void *memmove(void *dst, const void *src, size_t size) {
		return memcpy(dst, src, size); }


	/**
	 * Copy string
	 *
	 * \param dst   destination buffer
	 * \param src   buffer holding the null-terminated source string
	 * \param size  maximum number of characters to copy
	 * \return      pointer to destination string
	 *
	 * This function is not fully compatible to the C standard, in particular
	 * there is no zero-padding if the length of 'src' is smaller than 'size'.
	 * Furthermore, in contrast to the libc version, this function always
	 * produces a null-terminated string in the 'dst' buffer if the 'size'
	 * argument is greater than 0.
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
	 * \retval 0  memory blocks are equal
	 * \retval 1  memory blocks differ
	 *
	 * NOTE: This function is not fully compatible to the C standard.
	 */
	inline int memcmp(const void *p0, const void *p1, size_t size)
	{
		char *c0 = (char *)p0;
		char *c1 = (char *)p1;

		size_t i;
		for (i = 0; i < size; i++)
			if (c0[i] != c1[i]) return 1;

		return 0;
	}


	/**
	 * Memset
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
	 * Convert ASCII string to another type
	 *
	 * \param T       destination type of conversion
	 * \param s       null-terminated source string
	 * \param result  destination pointer to conversion result
	 * \param base    base, autodetected if set to 0
	 * \return        number of consumed characters
	 *
	 * Please note that 'base' and 's_max_len' are not evaluated by all
	 * template specializations.
	 */
	template <typename T>
	inline size_t ascii_to(const char *s, T *result, unsigned base = 0);


	/**
	 * Read unsigned long value from string
	 */
	template <>
	inline size_t ascii_to<unsigned long>(const char *s, unsigned long *result,
	                                      unsigned base)
	{
		unsigned long i = 0, value = 0;

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

		*result = value;
		return i;
	}


	/**
	 * Read unsigned int value from string
	 */
	template <>
	inline size_t ascii_to<unsigned int>(const char *s, unsigned int *result,
	                                     unsigned base)
	{
		unsigned long result_long = 0;
		size_t ret = ascii_to<unsigned long>(s, &result_long, base);
		*result = result_long;
		return ret;
	}


	/**
	 * Read signed long value from string
	 */
	template <>
	inline size_t ascii_to<long>(const char *s, long *result, unsigned base)
	{
		int i = 0;

		/* read sign */
		int sign = (*s == '-') ? -1 : 1;

		if (*s == '-' || *s == '+') { s++; i++; }

		int j = 0;
		unsigned long value = 0;

		j = ascii_to(s, &value, base);

		if (!j) return i;

		*result = sign*value;
		return i + j;
	}


	/**
	 * Wrapper of 'size_t' for selecting 'ascii_to' specialization
	 */
	class Number_of_bytes
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
	};


	/**
	 * Read 'Number_of_bytes' value from string and handle the size suffixes
	 *
	 * This function scales the resulting size value according to the suffixes
	 * for G (2^30), M (2^20), and K (2^10) if present.
	 */
	template <>
	inline size_t ascii_to(const char *s, Number_of_bytes *result, unsigned)
	{
		unsigned long res = 0;

		/* convert numeric part of string */
		int i = ascii_to(s, &res, 0);

		/* handle suffixes */
		if (i > 0)
			switch (s[i]) {
			case 'G': res *= 1024;
			case 'M': res *= 1024;
			case 'K': res *= 1024; i++;
			default: break;
			}

		*result = res;
		return i;
	}


	/**
	 * Read double float value from string
	 */
	template <>
	inline size_t ascii_to<double>(const char *s, double *result, unsigned)
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
			*result = neg ? -v : v;
			return i;
		}

		/* skip comma */
		i++;

		/* parse fractional part of number */
		for (; s[i] && is_digit(s[i]); i++, d *= 0.1)
			v += d*digit(s[i], false);

		*result = neg ? -v : v;
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

#endif /* _INCLUDE__UTIL__STRING_H_ */
