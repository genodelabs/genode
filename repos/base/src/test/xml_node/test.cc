/*
 * \brief  Test for XML parser
 * \author Norman Feske
 * \author Martin Stein
 * \date   2007-08-21
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/xml_node.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/log.h>

using namespace Genode;


/****************
 ** Test cases **
 ****************/

/* valid example of XML structure */
static const char *xml_test_valid =
	"<config>"
	"  <program>"
	"    <filename>init</filename>"
	"    <quota>16M</quota>"
	"  </program>"
	"  <!-- comment -->"
	"  <program>"
	"    <filename>timer</filename>"
	"    <quota>64K</quota>"
	"    <!-- <quota>32K</quota> -->"
	"  </program>"
	"  <program>"
	"    <filename>framebuffer</filename>"
	"    <quota>8M</quota>"
	"  </program>"
	"</config>";

/* the first 'program' tag is broken */
static const char *xml_test_broken_tag =
	"<config>"
	"  <program >"
	"    <filename>init</filename>"
	"    <quota>16M</quota>"
	"  </program>"
	"  <!-- comment -->"
	"  <program>"
	"    <filename>timer</filename>"
	"    <quota>64K</quota>"
	"  </program>"
	"  <program>"
	"    <filename>framebuffer</filename>"
	"    <quota>8M</quota>"
	"  </program>"
	"</config>";

/* end tag is missing */
static const char *xml_test_truncated =
	"<config>"
	"  <program >"
	"    <filename>init</filename>"
	"    <quota>16M</quota>"
	"  </program>"
	"  <!-- comment -->"
	"  <program>"
	"    <filename>timer</filename>"
	"    <quota>64K</quota>"
	"  </program>"
	"  <program>"
	"    <filename>framebuffer</filename>"
	"    <quota>8M</quota>"
	"  </program>";

/* comment end tag is missing */
static const char *xml_test_truncated_comment =
	"<config>"
	"  <program>"
	"    <filename>init</filename>"
	"    <quota>16M</quota>"
	"  </program>"
	"  <!-- comment -->"
	"  <program>"
	"    <filename>timer</filename>"
	"    <quota>64K</quota>"
	"    <!-- truncated comment"
	"  </program>"
	"  <program>"
	"    <filename>framebuffer</filename>"
	"    <quota>8M</quota>"
	"  </program>"
	"</config>";

/* contains unfinished string */
static const char *xml_test_unfinished_string =
	"<config>"
	"  <program>"
	"    <filename>init</filename>"
	"    <quota>16M</quota>"
	"  </program>"
	"  <!-- comment -->"
	"  <program>"
	"    <filename>\"unfinished string</filename>"
	"    <quota>64K</quota>"
	"  </program>"
	"  <program>"
	"    <filename>framebuffer</filename>"
	"    <quota>8M</quota>"
	"  </program>"
	"</config>";

/* valid XML structure attributes */
static const char *xml_test_attributes =
	"<config priolevels=\"4\">"
	"  <program>"
	"    <filename>init</filename>"
	"    <quota>16M</quota>"
	"  </program>"
	"  <single-tag/>"
	"  <single-tag-with-attr name=\"ein_name\" quantum=\"2K\" />"
	"</config>";

/* valid example of XML structure with text between nodes */
static const char *xml_test_text_between_nodes =
	"<config>"
	"  sometext1"
	"  <program attr=\"abcd\"/>"
	"  sometext2"
	"  <program>inProgram</program>"
	"  sometext3"
	"</config>";

/* strange but valid XML comments */
static const char *xml_test_comments =
	"<config>"
	"<visible-tag/>"
	"<!---->"
	"<!-- <invisible-tag/> -->"
	"<!--<invisible-tag/>-->"
	"<!--invisible-tag></invisible-tag-->"
	"<visible-tag/>"
	"</config>";


/******************
 ** Test program **
 ******************/

/**
 * Print attributes of XML token
 */
template <typename SCANNER_POLICY>
static const char *token_type_string(typename Token<SCANNER_POLICY>::Type token_type)
{
	switch (token_type) {
	case Token<SCANNER_POLICY>::SINGLECHAR: return "SINGLECHAR";
	case Token<SCANNER_POLICY>::NUMBER    : return "NUMBER";
	case Token<SCANNER_POLICY>::IDENT     : return "IDENT";
	case Token<SCANNER_POLICY>::STRING    : return "STRING";
	case Token<SCANNER_POLICY>::WHITESPACE: return "WHITESPACE";
	case Token<SCANNER_POLICY>::END       : return "END";
	}
	return "<invalid>";
}


/**
 * Print attributes of XML token
 */
template <typename SCANNER_POLICY>
static void log_xml_token_info(Token<SCANNER_POLICY> xml_token)
{
	static char content_buf[128];
	xml_token.string(content_buf, sizeof(content_buf));
	log("token type=\"", token_type_string<SCANNER_POLICY>(xml_token.type()), "\", "
	    "len=", xml_token.len(), ", content=\"", Cstring(content_buf), "\"");
}


template <typename SCANNER_POLICY>
static void log_xml_tokens(const char *xml_string)
{
	Token<SCANNER_POLICY> token(xml_string);
	while (token.type() != Token<SCANNER_POLICY>::END) {
		log_xml_token_info(token);
		token = token.next();
	}
}


struct Indentation
{
	unsigned const _spaces;

	Indentation(unsigned spaces) : _spaces(spaces) { }

	void print(Output &output) const
	{
		for (unsigned i = 0; i < _spaces; i++)
			Genode::print(output, " ");
	}
};


/**
 * Helper for the formatted output of XML attribute information
 */
struct Formatted_xml_attribute
{
	Xml_node::Attribute const _attr;
	unsigned            const _indent;

	Formatted_xml_attribute(Xml_node::Attribute attr, unsigned indent)
	: _attr(attr), _indent(indent) { }

	void print(Output &output) const
	{
		char value[32]; value[0] = 0;
		_attr.value(value, sizeof(value));

		Genode::print(output, Indentation(_indent),
		              "attribute name=\"", _attr.name(), "\", "
		              "value=\"", Cstring(value), "\"");
	}
};


/**
 * Print attributes of XML node
 */
static void print_xml_attr_info(Output &output, Xml_node node, int indent = 0)
{
	try {
		for (Xml_node::Attribute a = node.attribute(0U); ; a = a.next())
			print(output, Formatted_xml_attribute(a, indent), "\n");

	} catch (Xml_node::Nonexistent_attribute) { }
}


/**
 * Print information about XML node and its sub nodes
 *
 * \param xml_node  root fo XML sub tree to print
 * \param indent    current indentation level
 */
struct Formatted_xml_node
{
	Xml_node const _node;
	unsigned const _indent;

	Formatted_xml_node(Xml_node node, unsigned indent = 0)
	: _node(node), _indent(indent) { }

	void print(Output &output) const
	{
		using Genode::print;

		/* print node information */
		print(output, Indentation(_indent),
		      "XML node: name = \"", _node.type(), "\", ");
		if (_node.num_sub_nodes() == 0) {
			char buf[128];
			_node.value(buf, sizeof(buf));
			print(output, "leaf content = \"", Cstring(buf), "\"");
		} else
			print(output, "number of subnodes = ", _node.num_sub_nodes());

		print(output, "\n");

		print_xml_attr_info(output, _node, _indent + 2);

		/* print information of sub nodes */
		for (unsigned i = 0; i < _node.num_sub_nodes(); i++) {
			try {
				print(output, Formatted_xml_node(_node.sub_node(i), _indent + 2));
			} catch (Xml_node::Invalid_syntax) {
				print(output, "invalid syntax of sub node ", i, "\n");
			}
		}
	}
};


/**
 * Print content of sub node with specified type
 */
static void log_key(Xml_node node, char const *key)
{
	try {
		Xml_node sub_node = node.sub_node(key);
		char buf[32];
		sub_node.value(buf, sizeof(buf));
		log("content of sub node \"", key, "\" = \"", Cstring(buf), "\"");
	} catch (Xml_node::Nonexistent_sub_node) {
		log("sub node \"", key, "\" is not defined\n");
	} catch (Xml_node::Invalid_syntax) {
		log("invalid syntax of node \"", key, "\"");
	}
}


static void log_xml_info(const char *xml_string)
{
	try {
		log(Formatted_xml_node(Xml_node(xml_string)));
	} catch (Xml_node::Invalid_syntax) {
		log("string has invalid XML syntax\n");
	}
}


template <size_t max_content_sz>
static void test_decoded_content(Env        &env,
                                 unsigned    step,
                                 const char *xml_string,
                                 size_t      content_off,
                                 size_t      content_sz)
{
	log("step ", step);


	/*
	 * Test Xml_node::decoded_content
	 */

	size_t const buf_sz = content_sz + 1;
	Attached_ram_dataspace buf1_ds(env.ram(), env.rm(), buf_sz);
	Attached_ram_dataspace buf2_ds(env.ram(), env.rm(), buf_sz);
	char * const buf1 = buf1_ds.local_addr<char>();
	char * const buf2 = buf2_ds.local_addr<char>();

	Xml_node xml(xml_string);
	size_t sz = xml.decoded_content(buf1, max_content_sz);

	if (sz > content_sz)
		error("content decoding states to have accessed memory it was not allowed to");

	memcpy(buf2, &xml_string[content_off], min(content_sz, max_content_sz));

	if (memcmp(buf1, buf2, buf_sz)) {
		error("resulting string of Xml_node::decoded_content is erroneous");
		log("----- should be -----");
		log(Cstring(buf2));
		log("----- is -----");
		log(Cstring(buf1));
	}

	/*
	 * Test Xml_node::decoded_content<String<N> >
	 */

	enum { MAX_OUT_STRING_SZ = 256 };
	using Out_string = String<min(max_content_sz + 1, (size_t)MAX_OUT_STRING_SZ)>;
	Out_string str = xml.decoded_content<Out_string>();

	if (memcmp(str.string(), buf2, min(str.size(), buf_sz))) {
		error("resulting string of Xml_node::decoded_content<String<N> > is erroneous");
		log("----- should be -----");
		log(Cstring(buf2));
		log("----- is -----");
		log(str);
	}
}


void Component::construct(Genode::Env &env)
{
	log("--- XML-token test ---");
	log_xml_tokens<Scanner_policy_identifier_with_underline>(xml_test_text_between_nodes);

	log("--- XML-parser test ---");

	log("-- Test valid XML structure --");
	log_xml_info(xml_test_valid);

	log("-- Test invalid XML structure (broken tag) --");
	log_xml_info(xml_test_broken_tag);

	log("-- Test invalid XML structure (truncated) --");
	log_xml_info(xml_test_truncated);

	log("-- Test invalid XML structure (truncated comment) --");
	log_xml_info(xml_test_truncated_comment);

	log("-- Test invalid XML structure (unfinished string) --");
	log_xml_info(xml_test_unfinished_string);

	log("-- Test node access by key --");
	Xml_node prg(Xml_node(xml_test_valid).sub_node(0U));
	log_key(prg, "filename");
	log_key(prg, "quota");
	log_key(prg, "info");

	log("-- Test access to XML attributes --");
	log_xml_info(xml_test_attributes);

	log("-- Test parsing XML with nodes mixed with text --");
	log_xml_info(xml_test_text_between_nodes);

	log("-- Test parsing XML with comments --");
	log_xml_info(xml_test_comments);

	log("-- Test exporting decoded content from XML node --");
	test_decoded_content<~0UL>(env, 1, xml_test_comments, 8, 119);
	test_decoded_content<119 >(env, 2, xml_test_comments, 8, 119);
	test_decoded_content<11  >(env, 3, xml_test_comments, 8, 119);
	test_decoded_content<1   >(env, 4, xml_test_comments, 8, 119);
	test_decoded_content<0   >(env, 5, xml_test_comments, 8, 119);
	log("");

	log("--- End of XML-parser test ---");
	env.parent().exit(0);
}
