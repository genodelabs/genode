/*
 * \brief  Test for XML generator
 * \author Norman Feske
 * \date   2014-01-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <util/xml_generator.h>
#include <util/xml_node.h>

using Genode::size_t;


static size_t fill_buffer_with_xml(char *dst, size_t dst_len)
{
	Genode::Xml_generator xml(dst, dst_len, "config", [&]
	{
		xml.attribute("xpos", "27");
		xml.attribute("ypos", "34");

		xml.node("box", [&]()
		{
			xml.attribute("width",  "320");
			xml.attribute("height", "240");
		});
		xml.node("label", [&] ()
		{
			xml.attribute("name", "a test");
			xml.node("sub_label");
			xml.node("another_sub_label", [&] ()
			{
				xml.node("sub_sub_label");
			});
		});
		xml.attribute("verbose", "true");
	});

	return xml.used();
}


int main(int argc, char **argv)
{
	using Genode::printf;

	printf("--- XML generator test started ---\n");

	static char dst[1000];

	/*
	 * Good-case test (to be matched against a known-good pattern in the
	 * corresponding run script).
	 */
	size_t used = fill_buffer_with_xml(dst, sizeof(dst));
	printf("result:\n\n%s\nused %zd bytes\n", dst, used);

	/*
	 * Test buffer overflow
	 */
	try {
		fill_buffer_with_xml(dst, 20); }
	catch (Genode::Xml_generator::Buffer_exceeded) {
		printf("buffer exceeded (expected error)\n"); }

	/*
	 * Test the sanitizing of XML node content
	 */
	{
		/* generate pattern that contains all possible byte values */
		char pattern[256];
		for (unsigned i = 0; i < sizeof(pattern); i++)
			pattern[i] = i;

		/* generate XML with the pattern as content */
		Genode::Xml_generator xml(dst, sizeof(dst), "data", [&] () {
			xml.append_sanitized(pattern, sizeof(pattern)); });

		/* parse the generated XML data */
		Genode::Xml_node node(dst);

		/* obtain decoded node content */
		char decoded[sizeof(dst)];
		size_t const decoded_len = node.decoded_content(decoded, sizeof(decoded));

		/* compare result with original pattern */
		if (decoded_len != sizeof(pattern)) {
			printf("decoded content has unexpected length %zd\n", decoded_len);
			return 1;
		}
		if (Genode::memcmp(decoded, pattern, sizeof(pattern))) {
			printf("decoded content does not match original pattern\n");
			return 1;
		}
	}

	printf("--- XML generator test finished ---\n");

	return 0;
}

