/*
 * \brief  Test for XML generator
 * \author Norman Feske
 * \date   2014-01-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <base/log.h>
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
		xml.node("bool", [&] ()
		{
			xml.attribute("true",  true);
			xml.attribute("false", false);
		});
		xml.node("signed", [&] ()
		{
			xml.attribute("int",      -1);
			xml.attribute("long",     -2L);
			xml.attribute("longlong", -3LL);
		});
		xml.node("unsigned", [&] ()
		{
			xml.attribute("int",      1U);
			xml.attribute("long",     2UL);
			xml.attribute("longlong", 3ULL);
		});
	});

	return xml.used();
}


void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	log("--- XML generator test started ---");

	static char dst[1000];

	/*
	 * Good-case test (to be matched against a known-good pattern in the
	 * corresponding run script).
	 */
	size_t used = fill_buffer_with_xml(dst, sizeof(dst));
	log("result:\n\n", Cstring(dst), "\nused ", used, " bytes");

	/*
	 * Test buffer overflow
	 */
	try {
		fill_buffer_with_xml(dst, 20); }
	catch (Genode::Xml_generator::Buffer_exceeded) {
		log("buffer exceeded (expected error)"); }

	/*
	 * Test the sanitizing of XML node content
	 */
	{
		/* generate pattern that contains all possible byte values */
		char pattern[256];
		for (unsigned i = 0; i < sizeof(pattern); i++)
			pattern[i] = i;

		/* generate XML with the pattern as content */
		Xml_generator xml(dst, sizeof(dst), "data", [&] () {
			xml.append_sanitized(pattern, sizeof(pattern)); });

		/* parse the generated XML data */
		Xml_node node(dst);

		/* obtain decoded node content */
		char decoded[sizeof(dst)];
		size_t const decoded_len = node.decoded_content(decoded, sizeof(decoded));

		/* compare result with original pattern */
		if (decoded_len != sizeof(pattern)) {
			log("decoded content has unexpected length ", decoded_len);
			return;
		}
		if (memcmp(decoded, pattern, sizeof(pattern))) {
			log("decoded content does not match original pattern");
			return;
		}
	}

	log("--- XML generator test finished ---");
	env.parent().exit(0);
}

