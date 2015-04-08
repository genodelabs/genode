/*
 * \brief  Test for XML parser
 * \author Norman Feske
 * \date   2007-08-21
 */

#include <util/xml_node.h>
#include <base/printf.h>

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


/******************
 ** Test program **
 ******************/

/**
 * Print attributes of XML node
 */
static void print_xml_attr_info(Xml_node xml_node, int indent = 0)
{
	try {
		for (Xml_node::Attribute a = xml_node.attribute(0U); ; a = a.next()) {

			/* indentation */
			for (int i = 0; i < indent; i++)
				printf(" ");

			/* read attribute name and value */
			char name[32]; name[0] = 0;
			a.value(name, sizeof(name));
			char value[32]; value[0] = 0;
			a.value(value, sizeof(value));

			printf("attribute name=\"%s\", value=\"%s\"\n", name, value);
		}
	} catch (Xml_node::Nonexistent_attribute) { }
}


/**
 * Print information about XML node and its sub nodes
 *
 * \param xml_node  root fo XML sub tree to print
 * \param indent    current indentation level
 */
static void print_xml_node_info(Xml_node xml_node, int indent = 0)
{
	char buf[128];
	xml_node.type_name(buf, sizeof(buf));

	/* indentation */
	for (int i = 0; i < indent; i++)
		printf(" ");

	/* print node information */
	printf("XML node: name = \"%s\", ", buf);
	if (xml_node.num_sub_nodes() == 0) {
		xml_node.value(buf, sizeof(buf));
		printf("leaf content = \"%s\"\n", buf);
	} else
		printf("number of subnodes = %d\n",
		        xml_node.num_sub_nodes());

	print_xml_attr_info(xml_node, indent + 2);

	/* print information of sub nodes */
	for (unsigned i = 0; i < xml_node.num_sub_nodes(); i++) {
		try {
			Xml_node sub_node = xml_node.sub_node(i);
			print_xml_node_info(sub_node, indent + 2);
		} catch (Xml_node::Invalid_syntax) {
			printf("invalid syntax of sub node %d\n", i);
		}
	}
}


/**
 * Print content of sub node with specified type
 */
static void print_key(Xml_node node, const char *key)
{
	try {
		Xml_node sub_node = node.sub_node(key);
		char buf[32];
		sub_node.value(buf, sizeof(buf));
		printf("content of sub node \"%s\" = \"%s\"\n", key, buf);
	} catch (Xml_node::Nonexistent_sub_node) {
		printf("sub node \"%s\" is not defined\n", key);
	} catch (Xml_node::Invalid_syntax) {
		printf("invalid syntax of node \"%s\"\n", key);
	}
}


static void print_xml_info(const char *xml_string)
{
	try {
		print_xml_node_info(Xml_node(xml_string));
	} catch (Xml_node::Invalid_syntax) {
		printf("string has invalid XML syntax\n");
	}
}


int main()
{
	printf("--- XML-parser test ---\n");

	printf("-- Test valid XML structure --\n");
	print_xml_info(xml_test_valid);

	printf("-- Test invalid XML structure (broken tag) --\n");
	print_xml_info(xml_test_broken_tag);

	printf("-- Test invalid XML structure (truncated) --\n");
	print_xml_info(xml_test_truncated);

	printf("-- Test invalid XML structure (truncated comment) --\n");
	print_xml_info(xml_test_truncated_comment);

	printf("-- Test invalid XML structure (unfinished string) --\n");
	print_xml_info(xml_test_unfinished_string);

	printf("-- Test node access by key --\n");
	Xml_node prg(Xml_node(xml_test_valid).sub_node(0U));
	print_key(prg, "filename");
	print_key(prg, "quota");
	print_key(prg, "info");

	printf("-- Test access to XML attributes --\n");
	print_xml_info(xml_test_attributes);

	printf("--- End of XML-parser test ---\n");
	return 0;
}
