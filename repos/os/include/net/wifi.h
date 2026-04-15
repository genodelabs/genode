/*
 * \brief  Wifi Pstring utility
 * \author Josef Soentgen
 * \date   2026-04-15
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2 or later.
 */

#ifndef _NET__WIFI_H_
#define _NET__WIFI_H_

#include <base/output.h>
#include <util/string.h>

namespace Net {

namespace Wifi {

	using namespace Genode;

	/*
	 * As Pstrings are used for both, SSID and PSK, use the larger
	 * value. Use String as underlying type because SSID containg NUL
	 * are, although allowed by spec, still not supported.
	 */

	struct Decoded_pstring : String<  63+1> { };
	struct Encoded_pstring : String<4*63+1> { };

	inline bool allowed_char(char c)
	{
		bool const allowed = c != '"'
		                  && c != '&'
		                  && c != '|'
		                  && c != '\\';

		return (c >= ' ' && c <= '~' && allowed);
	}

	struct Hex_char
	{
		char str[5] { };

		void _byte2hex(char *dest, unsigned char b)
		{
			int i = 1;
			if (b < 16) { dest[i--] = '0'; }

			for (; b > 0; b /= 16) {
				unsigned char const v = b % 16;
				unsigned char const c = (v > 9) ? v + 'a' - 10 : v + '0';
				dest[i--] = (char)c;
			}
		}

		Hex_char(unsigned char c)
		{
			str[0] = '\\';
			str[1] = 'x';
			_byte2hex(&str[2], c);
		}

		char const *string() const { return str; }

		void print(Output &out) const { Genode::print(out, str); }
	};

	/**
	 * Encode an decoded Pstring
	 */
	inline Encoded_pstring from(Decoded_pstring const &decoded)
	{
		if (!decoded.valid())
			return Encoded_pstring();

		char  tmp[Encoded_pstring::size()] = { };
		char *p = tmp;

		char const *src = decoded.string();

		for (size_t i = 0; i < decoded.length() - 1; i++) {

			char const c = src[i];

			/*
			 * Copy printeable ASCII characters, minus the
			 * non-allowed ones, verbatim. All other bytes
			 * are encoded as '\xnn'.
			 */

			if (allowed_char((char)c)) {
				*p = c; p++;
			} else {
				Hex_char const xc(c);

				*p = xc.string()[0]; p++;
				*p = xc.string()[1]; p++;
				*p = xc.string()[2]; p++;
				*p = xc.string()[3]; p++;
			}
		}

		return Encoded_pstring((char const*)tmp);
	}

	/**
	 * Decode an encoded Pstring
	 *
	 * If the encoded Pstring is malformed an empty decoded
	 * Pstring is returned.
	 */
	inline Decoded_pstring from(Encoded_pstring const &encoded)
	{
		if (!encoded.valid())
			return Decoded_pstring();

		unsigned char  tmp[Decoded_pstring::size()] = { };
		unsigned char *p = tmp;

		auto _valid_hex = [&] (char c) -> bool {
			return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };

		auto _hex2byte = [&] (char h0, char h1) -> unsigned char
		{
			if (!_valid_hex(h0) || !_valid_hex(h1)) return 0;

			/* assign first and spare cast later */
			unsigned char const u0 = h0;
			unsigned char const u1 = h1;

			unsigned char const a = u0 > '9' ? u0 - 'a' + 10 : u0 - '0';
			unsigned char const b = u1 > '9' ? u1 - 'a' + 10 : u1 - '0';

			return (a << 4) | b;
		};

		char const *src = encoded.string();

		for (size_t i = 0; i < encoded.length() - 1; i++, p++) {

			/*
			 * Decode '\xnn' and bail entirely in case the
			 * sequence is malformed. For the current validation
			 * to pass encountering '\x[0-9a-f][0-9a-f]' but not
			 * '\x00' is sufficient.
			 */

			if (src[i] != '\\') {
				*p = src[i];
			} else {

				if ( src[i+1] != 'x'
				  /* truncated sequence */
				  || (i + 4) >= encoded.length()
				  /* malformed or encoded NUL */
				  || (*p = _hex2byte(src[i+2], src[i+3])) == 0)
					return Decoded_pstring();

				/* skip parsed bytes */
				i += 3;
			}
		}

		return Decoded_pstring((char const*)tmp);
	}

} /* namespace Wifi */

} /* namespace Net */

#endif /* _NET__WIFI_H_ */
