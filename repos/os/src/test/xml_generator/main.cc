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
	printf("result:\n\n%s\n\nused %zd bytes\n", dst, used);

	/*
	 * Test buffer overflow
	 */
	try {
		fill_buffer_with_xml(dst, 20); }
	catch (Genode::Xml_generator::Buffer_exceeded) {
		printf("buffer exceeded (expected error)\n"); }

	printf("--- XML generator test finished ---\n");

	return 0;
}

