/*
 * \brief  Test for UTF-8 decoder
 * \author Norman Feske
 * \date   2018-03-15
 *
 * This test is based on the "UTF-8 decoder capability and stress test" by
 * Markus Kuhn: http://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
#include <util/utf8.h>

using namespace Genode;


struct Expect_codepoint
{
	unsigned const _expected;

	Expect_codepoint(unsigned expected) : _expected(expected) { }

	bool test(Utf8_ptr const &utf8) const
	{
		unsigned const value = utf8.codepoint().value;

		if (value == _expected)
			return true;

		error("expected codepoint ", Hex(_expected), ", got ", Hex(value));
		return false;
	}
};


struct Expect_invalid
{
	bool test(Utf8_ptr const &utf8) const
	{
		if (!utf8.codepoint().valid())
			return true;

		error("expected invalid codepoint, got ", Hex(utf8.codepoint().value));
		return false;
	}
};


struct Expect_end
{
	static bool test(Utf8_ptr const &utf8)
	{
		if (!utf8.complete())
			return true;

		error("expected incomplete UTF-8 sequence");
		return false;
	}
};


/**
 * Exception type
 */
struct Failed { };


template <typename HEAD, typename... TAIL>
void test(Utf8_ptr const &utf8, HEAD const &head, TAIL &... tail)
{
	if (!head.test(utf8))
		throw Failed();

	test(utf8.next(), tail...);
}


template <typename HEAD>
void test(Utf8_ptr const &utf8, HEAD const &head)
{
	if (!head.test(utf8))
		throw Failed();
}


template <typename... ARGS>
void test(void const *s, ARGS &&... args)
{
	test(Utf8_ptr((char const *)s), args...);
}


void Component::construct(Genode::Env &env)
{
	/* 1 */
	test("κόσμε", Expect_codepoint(0x3ba),
	              Expect_codepoint(0x1f79),
	              Expect_codepoint(0x3c3),
	              Expect_codepoint(0x3bc),
	              Expect_codepoint(0x3b5));

	/* 2.1.1  1 byte  (U-00000000) */
	test("�", Expect_invalid(), Expect_end());

	/* 2.1.2  2 bytes (U-00000080) */
	test("\xc2\x80", Expect_codepoint(0x80), Expect_end());

	/* 2.1.3  3 bytes (U-00000800) */
	test("\xe0\xa0\x80", Expect_codepoint(0x800), Expect_end());

	/* 2.1.4  4 bytes (U-00010000) */
	test("\xf0\x90\x80\x80", Expect_codepoint(0x10000), Expect_end());

	/*
	 * Skipped because 'Genode::Utf8' does not handle sequences of more than
	 * four bytes.
	 *
	 * 2.1.5  5 bytes (U-00200000)
	 * 2.1.6  6 bytes (U-04000000)
	 */

	/* 2.2.1  1 byte  (U-0000007F) */
	test("\x7f", Expect_codepoint(0x7f), Expect_end());

	/* 2.2.2  2 bytes (U-000007FF) */
	test("\xdf\xbf", Expect_codepoint(0x7ff), Expect_end());

	/* 2.2.3  3 bytes (U-0000FFFF) */
	test("\xef\xbf\xbf", Expect_codepoint(0xffff), Expect_end());

	/*
	 * 2.2.4  4 bytes (U-001FFFFF)
	 *
	 * Adjusted to valid range of Unicode codepoints
	 */
	test("\xf4\x8f\xbf\xbf", Expect_codepoint(0x10ffff), Expect_end());

	/*
	 * Skipped, see 2.1.5
	 *
	 * 2.2.5  5 bytes (U-03FFFFFF)
	 * 2.2.6  6 bytes (U-7FFFFFFF)
	 */

	/* 2.3  Other boundary conditions */

	/* 2.3.1  U-0000D7FF = ed 9f bf */
	test("\xed\x9f\xbf", Expect_codepoint(0xd7FF), Expect_end());

	/* 2.3.2  U-0000E000 = ee 80 80 */
	test("\xee\x80\x80", Expect_codepoint(0xe000), Expect_end());

	/* 2.3.3  U-0000FFFD = ef bf bd */
	test("\xef\xbf\xbd", Expect_codepoint(0xfffd), Expect_end());

	/* 2.3.4  U-0010FFFF = f4 8f bf bf */
	test("\xf4\x8f\xbf\xbf", Expect_codepoint(0x10ffff), Expect_end());

	/*
	 * 2.3.5  U-00110000 = f4 90 80 80
	 *
	 * Outside the valid range of Unicode codepoints
	 */
	test("\xf4\x90\x80\x80", Expect_invalid(), Expect_end());

	/* 3  Malformed sequences */
	/* 3.1  Unexpected continuation bytes */

	/* 3.1.1  First continuation byte 0x80 */
	test("\x80", Expect_invalid(), Expect_end());

	/* 3.1.2  Last  continuation byte 0xbf */
	test("\xbf", Expect_invalid(), Expect_end());

	/* 3.1.3  2 continuation bytes */
	test("\xbf\xbf", Expect_invalid(), Expect_invalid(), Expect_end());

	/* 3.1.4  3 continuation bytes */
	test("\xbf\xbf\xbf", Expect_invalid(), Expect_invalid(),
	                     Expect_invalid(), Expect_end());

	/*
	 * Skipped because the 'Genode::Utf8_ptr' handles each 0xbf as a separate
	 * sequence (as shown above).
	 *
	 * 3.1.5  4 continuation bytes
	 * 3.1.6  5 continuation bytes
	 * 3.1.7  6 continuation bytes
	 * 3.1.8  7 continuation bytes
	 */

	/* 3.1.9  Sequence of all 64 possible continuation bytes (0x80-0xbf) */
	{
		enum { START = 0x80, END = 0xbf };
		for (unsigned char i = START; i <= END; i++) {
			unsigned char text[] = { i, 0 };
			test(text, Expect_invalid(), Expect_end());
		}
	}

	/* 3.2  Lonely start characters */

	/* 3.2.1  All 32 first bytes of 2-byte sequences (0xc0-0xdf)
	          each followed by a space character */

	for (unsigned char i = 0xc0; i <= 0xdf; i++) {
		unsigned char text[] = { i, ' ', 0 };
		test(text, Expect_invalid(), Expect_codepoint(' '), Expect_end());
	}

	/* 3.2.2  All 16 first bytes of 3-byte sequences (0xe0-0xef)
	          each followed by a space character */

	for (unsigned char i = 0xe0; i <= 0xef; i++) {
		unsigned char text[] = { i, ' ', 0 };
		test(text, Expect_invalid(), Expect_codepoint(' '), Expect_end());
	}

	/* 3.2.3  All 8 first bytes of 4-byte sequences (0xf0-0xf7)
	          each followed by a space character */

	for (unsigned char i = 0xf0; i <= 0xf7; i++) {
		unsigned char text[] = { i, ' ', 0 };
		test(text, Expect_invalid(), Expect_codepoint(' '), Expect_end());
	}

	/*
	 * Skipped, see 2.1.5
	 *
	 * 3.2.4  All 4 first bytes of 5-byte sequences (0xf8-0xfb)
	 * 3.2.5  All 2 first bytes of 6-byte sequences (0xfc-0xfd)
	 */

	/*
	 * 3.3  Sequences with last continuation byte missing
	 *
	 * 3.3.1  2-byte sequence with last byte missing (U+0000)
	 * 3.3.2  3-byte sequence with last byte missing (U+0000)
	 * 3.3.3  4-byte sequence with last byte missing (U+0000)
	 * 3.3.4  5-byte sequence with last byte missing (U+0000)
	 * 3.3.5  6-byte sequence with last byte missing (U+0000)
	 * 3.3.6  2-byte sequence with last byte missing (U-000007FF)
	 * 3.3.7  3-byte sequence with last byte missing (U-0000FFFF)
	 * 3.3.8  4-byte sequence with last byte missing (U-001FFFFF)
	 * 3.3.9  5-byte sequence with last byte missing (U-03FFFFFF)
	 * 3.3.10 6-byte sequence with last byte missing (U-7FFFFFFF)
	 *
	 * The following test starts a three-byte sequence but has a space
	 * instead of third byte. The 'Genode::Utf8_ptr' steps over the
	 * malformed sequence, detecting the valid space character.
	 */
	test("\xef\xbf ", Expect_invalid(), Expect_codepoint(' '), Expect_end());

	/*
	 * 3.4  Concatenation of incomplete sequences
	 *
	 * The test interrupts a three-byte sequence after the second byte
	 * with a new (valid) three-byte sequence.
	 */
	test("\xef\xbf\xef\xbf\xbf",
	     Expect_invalid(), Expect_codepoint(0xffff), Expect_end());

	/*
	 * 3.5  Impossible bytes
	 */

	/* 3.5.1  fe */
	test("\xfe", Expect_invalid(), Expect_end());

	/* 3.5.2  ff */
	test("\xff", Expect_invalid(), Expect_end());

	/* 3.5.3  fe fe ff ff */
	test("\xfe\xfe\xff\xff", Expect_invalid(), Expect_invalid(),
	                         Expect_invalid(), Expect_invalid(),
	                         Expect_end());

	/* 4  Overlong sequences */

	/* 4.1.1 U+002F = c0 af */
	test("\xc0\xaf", Expect_invalid(), Expect_end());

	/* 4.1.2 U+002F = e0 80 af */
	test("\xe0\x80\xaf", Expect_invalid(), Expect_end());

	/* 4.1.3 U+002F = f0 80 80 af */
	test("\xf0\x80\x80\xaf", Expect_invalid(), Expect_end());

	/*
	 * 4.1.4 U+002F = f8 80 80 80 af
	 * 4.1.5 U+002F = fc 80 80 80 80 af
	 *
	 * The 'Genode::Utf8_ptr' implementation consumes the first four bytes
	 * as one invalid sequence, and the trailing 0xaf as another invalid
	 * sequence.
	 */
	test("\xf0\x80\x80\x80\xaf", Expect_invalid(), Expect_invalid(), Expect_end());

	/* 4.2  Maximum overlong sequences */

	/* 4.2.1  U-0000007F = c1 bf */
	test("\xc1\xbf", Expect_invalid(), Expect_end());

	/* 4.2.2  U-000007FF = e0 9f bf */
	test("\xe0\x9f\xbf", Expect_invalid(), Expect_end());

	/* 4.2.3  U-0000FFFF = f0 8f bf bf */
	test("\xf0\x8f\xbf\xbf", Expect_invalid(), Expect_end());

	/*
	 * 4.2.4  U-001FFFFF = f8 87 bf bf bf
	 * 4.2.5  U-03FFFFFF = fc 83 bf bf bf bf
	 *
	 * Skipped, see 2.1.5
	 */

	/* 4.3  Overlong representation of the NUL character */

	/* 4.3.1  U+0000 = c0 80 */
	test("\xc0\x80", Expect_invalid(), Expect_end());

	/* 4.3.2  U+0000 = e0 80 80 */
	test("\xe0\x80\x80", Expect_invalid(), Expect_end());

	/* 4.3.3  U+0000 = f0 80 80 80 */
	test("\xf0\x80\x80\x80", Expect_invalid(), Expect_end());

	/*
	 * 4.3.4  U+0000 = f8 80 80 80 80
	 * 4.3.5  U+0000 = fc 80 80 80 80 80
	 *
	 * Skipped, see 2.1.5
	 */

	/* 5  Illegal code positions */

	/* 5.1  Single UTF-8 surrogates */
	test("\xed\xa0\x80", Expect_invalid(), Expect_end());
	test("\xed\xad\xbf", Expect_invalid(), Expect_end());
	test("\xed\xae\x80", Expect_invalid(), Expect_end());
	test("\xed\xaf\xbf", Expect_invalid(), Expect_end());
	test("\xed\xb0\x80", Expect_invalid(), Expect_end());
	test("\xed\xbe\x80", Expect_invalid(), Expect_end());
	test("\xed\xbf\xbf", Expect_invalid(), Expect_end());

	/* 5.2 Paired UTF-16 surrogates */

	test("\xed\xa0\x80\xed\xb0\x80", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xa0\x80\xed\xbf\xbf", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xad\xbf\xed\xb0\x80", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xad\xbf\xed\xbf\xbf", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xae\x80\xed\xb0\x80", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xae\x80\xed\xbf\xbf", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xaf\xbf\xed\xb0\x80", Expect_invalid(), Expect_invalid(), Expect_end());
	test("\xed\xaf\xbf\xed\xbf\xbf", Expect_invalid(), Expect_invalid(), Expect_end());

	/* 5.3 Noncharacter code positions */

	/* 5.3.1  U+FFFE = ef bf be */
	test("\xef\xbf\xbe", Expect_invalid(), Expect_end());

	/*
	 * 5.3.2  U+FFFF = ef bf bf
	 *
	 * Skipped because discarding 0xffff would contradict with 2.2.3
	 */

	/* 5.3.3  U+FDD0 .. U+FDEF */
	for (unsigned char i = 0x90; i <= 0xaf; i++) {
		unsigned char text[] = { 0xef, 0xf7, i };
		test(text, Expect_invalid(), Expect_end());
	}

	/* 5.3.4  U+nFFFE U+nFFFF (for n = 1..10) */
	for (unsigned char i = 0x90; i <= 0xaf; i++) {
		unsigned char text[] = { 0xef, 0xf7, i };
		test(text, Expect_invalid(), Expect_end());
	}

	env.parent().exit(0);
}

