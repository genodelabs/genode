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


static size_t xml_with_exceptions(char *dst, size_t dst_len)
{
	Genode::Xml_generator xml(dst, dst_len, "config", [&]
	{
		xml.node("level1", [&] ()
		{
			xml.node("level2", [&] ()
			{
				xml.attribute("attr1", 0x87654321ULL);
				for (unsigned i=0; i < 3; i++) {
					try {
						xml.node("level3_exception", [&] ()
						{
							xml.attribute("attr1", 1234);
							xml.attribute("attr2", 4321);
							xml.attribute("attr3", 2143);
							xml.node("level4_exception", [&] ()
							{
								xml.attribute("attr1", "Hallo");
								xml.node("level5_exception_1", [&] ()
								{
									xml.attribute("attr1", true);
									xml.attribute("attr2", false);
								});
								xml.node("level5_exception_2", [&] () { });
								throw 10 + i;
							});
						});
					} catch (unsigned error) {
						Genode::log("exception with value ", error, " on level 4 (expected error)");
					}
					xml.node("level3", [&] ()
					{
						xml.attribute("attr1", "Hallo");
						xml.attribute("attr2", 123000 + i);
						xml.node("level4_1", [&] () {
							xml.attribute("attr1", true);
							xml.attribute("attr2", "Welt");
						});
						try {
							xml.node("level4_exception", [&] ()
							{
								xml.attribute("attr1", "Welt");
								xml.attribute("attr2", 2143);
								xml.attribute("attr3", false);
								xml.attribute("attr3", 0x12345678ULL);
								xml.node("level5_exception_1", [&] () { });
								xml.node("level5_exception_2", [&] () { });
								xml.node("level5_exception_3", [&] ()
								{
									xml.node("level6_exception", [&] ()
									{
										xml.attribute("attr1", 0x12345678ULL);
										xml.node("level7_exception_3", [&] ()
										{
											xml.node("level8_exception_1", [&] () { });
											xml.node("level8_exception_2", [&] () { });
											xml.node("level8_exception_3", [&] () { });
											xml.node("level8_exception_4", [&] ()
											{
												throw 20 + i;
											});
										});
									});
								});
							});
						} catch (unsigned error) {
							Genode::log("exception with value ", error, " on level 8 (expected error)");
						}
						xml.node("level4_2", [&] () { });
						try {
							xml.node("level4_exception", [&] ()
							{
								xml.attribute("attr1", "Welt");
								xml.attribute("attr2", 2143);
								throw 30 + i;
							});
						} catch (unsigned error) {
							Genode::log("exception with value ", error, " on level 4 (expected error)");
						}
					});
				}
			});
			try {
				xml.node("level2_exception", [&] ()
				{
					throw 40;
				});
			} catch (int error) {
				Genode::log("exception with value ", error, " on level 2 (expected error)");
			}
		});
	});
	return xml.used();
}

extern void gcov_init(Genode::Env &env);
extern void genode_exit(int status);

void Component::construct(Genode::Env &env)
{
	using namespace Genode;

	log("--- XML generator test started ---");

	env.exec_static_constructors();
	gcov_init(env);

	static char dst[1000];

	/*
	 * Good-case test (to be matched against a known-good pattern in the
	 * corresponding run script).
	 */
	size_t used = fill_buffer_with_xml(dst, sizeof(dst));
	log("\nused ", used, " bytes, result:\n\n", Cstring(dst));

	/*
	 * Test buffer overflow
	 */
	try {
		fill_buffer_with_xml(dst, 20); }
	catch (Genode::Xml_generator::Buffer_exceeded) {
		log("buffer exceeded (expected error)\n"); }

	/*
	 * Test throwing non-XML related exceptions during xml generation
	 */
	memset(dst, 0, sizeof(dst));
	used = xml_with_exceptions(dst, sizeof(dst));
	log("\nused ", used, " bytes, result:\n\n", Cstring(dst));

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

	/*
	 * Test arbitrary content
	 */
	{
		Xml_generator xml(dst, sizeof(dst), "data", [&] () {
			xml.append_content(" ", 2 + 2, " == 2 + 2 == ", 4.0, " ");
		});

		Xml_node node(dst);
		auto s = node.decoded_content<String<32>>();
		if (s != " 4 == 2 + 2 == 4.0 ") {
			error("decoded content does not match expect content");
			return;
		}
	}

	log("--- XML generator test finished ---");
	genode_exit(0);
}

