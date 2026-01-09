/*
 * \brief  Test for parsing human-inclined data (HID)
 * \author Norman Feske
 * \date   2025-06-11
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/hid.h>
#include <util/formatted_output.h>
#include <base/component.h>
#include <base/log.h>
#include <base/sleep.h>

using namespace Genode;


/* valid example of node structure */
static const char *good_case_test =
	"launcher pkg: genodelabs/pkg/record_play_mixer/2024-12-10 | priority: 0\n"
	"+ config jitter_ms: 10 | warning_rate_ms: 0\n"
	"  .\n"
	"  . These default wildcard rules match all regular play clients.\n"
	"  . Please check the mixer report to replace the fuzzy configuration\n"
	"  . with absolute labels in case you want to discern multiple clients.\n"
	"  .\n"
	"  + mix left\n"              /* name attribute w/o tag */
	"  | + play label_suffix: -> left  | volume: 0.5\r\n"  /* CRLF */
	"  + mix right\n"
	"  | + play label_suffix: -> right | volume: 0.5\n"
	"  .\n"
	"  . capture \tmicrophone\n"  /* TAB in comment */
	"  .\n"
	"  x mix mic_left\n"          /* disabled node and sub node */
	"  | + play label: audio -> mic_left\n"
	"  x mix mic_right\n"
	"  | + play label: audio -> mic_right\n"
	"  .\n"
	"  . rule for the vbox6 VM matching its OSS configuration\n"
	"  .\n"
	"  x policy label: vbox6 -> vbox -> left  | record: mic_left\n"
	"  |                                        period_ms: 46\n"
	"  |                                        jitter_ms: 23\n"
	"  |                                        volume: 1.0\n"
	"  x policy label: vbox6 -> vbox -> right | record: mic_right\n"
	"  |                                        period_ms: 46\n"
	"  |                                        jitter_ms: 23\n"
	"  |                                        volume: 1.0\n"
	"  .\n"
	"  . rule for the audio driver matching its configuration\n"
	"  .\n"
	"  + policy label: audio -> left  | record: left\n"
	"  |                                period_ms: 12\n"
	"  |                                jitter_ms:  5\n"
	"  |                                volume:   1.0\n"
	"  + policy label: audio -> right | record: right\n"
	"                                   period_ms: 12\n"
	"                                   jitter_ms:  5\n"
	"                                   volume:   1.0\n"
	"    : ...some additional\n"
	"    : free-form\n"
	"    : content \tprefixed \n" /* TAB within raw segment */
	"    : with :\n"
	"+ route | + service Report | + parent\n"
	"-";


struct Indentation
{
	unsigned n;

	void print(Output &out) const { Genode::print(out, Repeated(n, " ")); }
};


/**
 * Helper for the formatted output of attribute information
 */
struct Formatted_attribute
{
	Hid_node::Attribute const &_attr;
	unsigned            const  _indent;

	Formatted_attribute(Hid_node::Attribute const &attr, unsigned indent)
	: _attr(attr), _indent(indent) { }

	void print(Output &output) const
	{
		Genode::print(output, Indentation(_indent),
		              "attribute name=\"", Cstring(_attr.tag.start, _attr.tag.num_bytes), "\", "
		              "value=\"", Cstring(_attr.value.start, _attr.value.num_bytes), "\"");
	}
};


/**
 * Print attributes of node
 */
static void print_attr_info(Output &output, Hid_node const &node, int indent = 0)
{
	node.for_each_attribute([&] (Hid_node::Attribute const &a) {
		print(output, Formatted_attribute(a, indent), "\n"); });
}


/**
 * Print information about node and its sub nodes
 *
 * \param node    root fo sub tree to print
 * \param indent  current indentation level
 */
struct Formatted_node
{
	Hid_node const &_node;
	unsigned const  _indent;

	Formatted_node(Hid_node const &node, unsigned indent = 0)
	: _node(node), _indent(indent)
	{ }

	void print(Output &out) const
	{
		using Genode::print;

		/* print node information */
		print(out, Indentation(_indent),
		      "node: type = \"", _node.type(), "\"");
		if (_node.num_sub_nodes() == 0) {
			unsigned lines = 0;
			_node.for_each_quoted_line([&] (auto const &) { lines++; });
			if (lines) {
				print(out, ", leaf content = \"");
				_node.for_each_quoted_line([&] (auto const &line) {
					print(out, line, " "); });
				print(out, "\"");
			}
		} else {
			print(out, ", number of subnodes = ", _node.num_sub_nodes());
		}

		print(out, "\n");

		print_attr_info(out, _node, _indent + 2);

		/* print information of sub nodes */
		_node.for_each_sub_node([&] (Hid_node const &node) {
			print(out, Formatted_node(node, _indent + 2)); });
	}
};


void Component::construct(Genode::Env &env)
{
	auto fail = [&] (auto &&... msg)
	{
		error(msg...);
		env.parent().exit(-1);
		sleep_forever();
	};

	auto expect_invalid = [&] (char const *invalid)
	{
		if (Hid_node { { invalid, strlen(invalid) } }.type() != "invalid")
			fail("accepted invalid input: '", invalid, "'");
	};

	log("--- HID-parser test ---");

	Const_byte_range_ptr const bytes { good_case_test, strlen(good_case_test) };

	log(Formatted_node(Hid_node { bytes }));

	/* truncation */
	for (size_t n = 0; n < bytes.num_bytes; n++)
		if (Hid_node { { good_case_test, n } }.type() != "invalid")
			fail("truncated HID node undetected");

	/* TAB at wrong places */
	expect_invalid("launcher\ttest: 1\n-");   /* tab wrongly used as separator */
	expect_invalid("launcher test:\t2\n-");
	expect_invalid("launcher\n\ttest: 3\n-"); /* tab wrongly used or indentation */

	/* CR at wrong places */
	expect_invalid("launcher\n\r  test: 4\n-");
	expect_invalid("launcher\n  test:\r 5\n-");

	/* reject control characters */
	for (char i = 0; i < 0x20; i++)
		if (i != '\n')
			expect_invalid(String<100>("launcher i: ", i, " | tag: ", Char(i), " \n-").string());

	/* ignore content of disabled node */
	{
		char const * const test = "config\n"
		                         "+ start black_hole\n"
		                         "x start system_shell | ram: 1G\n"
		                         "-";
		Hid_node(Span { test, strlen(test) }).for_each_sub_node([&] (Hid_node const &node) {
			if (node.attribute_value("ram", String<16>("nix")) != "nix")
				fail("unexpected use of attribute of disabled node"); });
	}

	/* clip span when parsing attribute value */
	{
		struct Server
		{
			String<64> name;

			size_t parse(Span const &s)
			{
				name = { Cstring(s.start, s.num_bytes) };
				return s.num_bytes;
			}
		};

		char const * const test = "config server: genode.org | port: 80\n-";

		Server const server = Hid_node(Span { test, strlen(test) })
			.attribute_value("server", Server { });

		if (server.name != "genode.org")
			fail("unexpected attr value in span-clip test: '", server.name, "'");
	}

	auto with_generated = [&] (auto const &node_type, auto const &fn, auto const &result_fn)
	{
		char buf[4*1024] { };
		Hid_generator::generate({ buf, sizeof(buf)}, node_type, fn).with_result(
			[&] (size_t num_bytes) { result_fn(Node(Span { buf, num_bytes })); },
			[&] (Buffer_error) { }
		);
	};

	auto print_generated = [&] (auto const &node_type, auto const &fn)
	{
		with_generated(node_type, fn, [&] (Node const &node) { log(node); });
	};

	/*
	 * preserved comments and formatting
	 */
	print_generated("verbatim_copy", [&] (Hid_generator &g) {

		char const * const node_with_comments =
		"launcher\n"
		"+ config\n"
		"  + vfs\n"
		"    .\n"
		"    . list of overlayed tar archives\n"
		"    .\n"
		"    + tar vim.tar\n"
		"\n"
		"    + dir dev\n"
		"      .\n"
		"      . pseudo devices used by libc\n"
		"      .\n"
		"      + log\n"
		"      + rtc\n"
		"-";
		Hid_node const node { { node_with_comments, strlen(node_with_comments) } };

		node.with_sub_node("config", [&] (Hid_node const &node) {
			node.with_sub_node("vfs", [&] (Hid_node const &node) {
				g.append_node(node);
			}, [] { });
		}, [] { });
	});

	/*
	 * tabular data aligned at nested nodes
	 *
	 * route
	 * + service Timer                        | + child timer
	 * + service Event                        | + child nitpicker
	 * + service ROM | label: config          | + child config_fs_rom | label: managed/event_filter
	 * + service ROM | label_prefix: keyboard | + child config_fs_rom
	 * + service ROM | label: numlock.remap   | + child numlock_remap_rom
	 * + service ROM | label: capslock        | + child report_rom
	 * + service ROM                          | + parent
	 * + service PD                           | + parent
	 * + service CPU                          | + parent
	 * + service LOG                          | + parent
	 */
	print_generated("tabular_nested_nodes", [&] (Hid_generator &g) {

		auto gen_service_node = [&] (auto const &service, auto const &fn)
		{
			g.node("service", [&] {
				g.attribute("name", service);
				fn(); });
		};

		auto gen_named_node = [&] (auto const &type, auto const &name, auto const &fn)
		{
			g.node(type, [&] {
				g.attribute("name", name);
				fn(); });
		};

		auto gen_parent_route = [&] (auto const &service)
		{
			g.node("service", [&] {
				g.attribute("name", service);
				g.node("parent", [&] { }); });
		};

		g.node("start", [&] {
			g.tabular([&] {

				gen_service_node("Timer", [&] {
					gen_named_node("child", "timer", [&] { }); });

				gen_service_node("Event", [&] {
					gen_named_node("child", "nitpicker", [&] { }); });

				gen_service_node("ROM", [&] {
					g.attribute("label", "config");
					gen_named_node("child", "config_fs_rom", [&] {
						g.attribute("label", "managed/event_filter"); }); });

				gen_service_node("ROM", [&] {
					g.attribute("label_prefix", "keyboard");
					gen_named_node("child", "config_fs_rom", [&] { }); });

				gen_service_node("ROM", [&] {
					g.attribute("label", "numlock.remap");
					gen_named_node("child", "numlock_remap_rom", [&] { }); });

				gen_service_node("ROM", [&] {
					g.attribute("label", "capslock");
					gen_named_node("child", "report_rom", [&] { }); });

				gen_parent_route("ROM");
				gen_parent_route("PD");
				gen_parent_route("CPU");
				gen_parent_route("LOG");
			});
		});
	});

	/*
	 * tabular data with aligned attributes
	 *
	 * Attributes are aligned as a table as long as all tags in a column have
	 * the same length and all node types have the same length. Optional
	 * trailing attributes are not aligned.
	 */
	print_generated("tabular_attributes", [&] (Hid_generator &g) {

		/* graceful handling of optional attributes */
		g.node("views", [&] {
			auto gen_view = [&] (auto n, auto x, auto y, auto w, auto h)
			{
				g.node("view", [&] {
					if (n >= 0) g.attribute("xpos",   x);
					if (n >= 1) g.attribute("ypos",   y);
					if (n >= 2) g.attribute("width",  w);
					if (n >= 3) g.attribute("height", h);
				});
			};
			g.tabular([&] {
				for (unsigned i = 0; i < 10; i++)
					gen_view(min(9-i, 3u), 108 - i*12, i*i*i*i*i, (i*5454) % 99999, i);
			});
		});

		/* alignment of tag-less name attribute */
		g.node("points", [&] {
			auto gen_named_point = [&] (auto name, auto x, auto y)
			{
				g.node("point", [&] {
					g.attribute("name", name);
					g.attribute("xpos", x);
					g.attribute("ypos", y);
				});
			};
			g.tabular([&] {
				for (unsigned i = 0; i < 10; i++)
					gen_named_point(i&1 ? "odd" : "even", 108 - i*12, i*i*i*i);
			});
		});

		/* render w and h attributes densely because w contradicts outer_radius */
		g.node("shapes", [&] {
			g.tabular([&] {
				g.node("shape", [&] {
					g.attribute("name", "point");
					g.attribute("x", 0);
					g.attribute("y", 100);
				});
				g.node("shape", [&] {
					g.attribute("name", "circle");
					g.attribute("x", 100);
					g.attribute("y", 1);
					g.attribute("outer_radius", 75);
				});
				g.node("shape", [&] {
					g.attribute("name", "rect");
					g.attribute("x", 50);
					g.attribute("y", 5);
					g.attribute("w", 15);
					g.attribute("h", 35);
				});
			});
		});

		/* fall back to dense formating if number of attributes exceeds 8 */
		g.tabular([&] {
			g.node("wide", [&] {
				for (unsigned i = 0; i < 9; i++)
					g.attribute(String<15>(Char('a' + char(i))).string(), i); });
			g.node("wide", [&] {
				for (unsigned i = 0, j = 1; i < 9; i++, j *= 2)
					g.attribute(String<15>(Char('a' + char(i))).string(), j); });
		});
	});

	print_generated("quoted_content", [&] (Hid_generator &g) {
		g.node("env", [&] {
			g.attribute("name", "PS1");
			g.append_quoted("system:$PWD> ");
		});
		g.node("env", [&] {
			g.attribute("name", "SHELL");
			g.append_quoted("/bin/bash");
		});
		g.node("tictactoe", [&] {
			g.append_quoted("X 0 X\n");
			g.append_quoted("X . .\n");
			g.append_quoted("0 X 0");
		});
		g.node("t_i_c_t_a_c_t_o_e", [&] {
			g.append_quoted("\nX  0  X\n\n");
			g.append_quoted(  "X  .  .\n\n");
			g.append_quoted(  "0  X  0\n");
		});
		g.node("piecewise", [&] {
			for (char c = 'a'; c <= 'z'; c++)
				g.append_quoted(String<8>(Char(c)));
		});
	});

	print_generated("tabular_quoted_content", [&] (Hid_generator &g) {
		g.tabular([&] {
			g.node("env", [&] {
				g.attribute("name", "PS1");
				g.append_quoted("system:$PWD> ");
			});
			g.node("env", [&] {
				g.attribute("name", "SHELL");
				g.append_quoted("/bin/bash");
			});
		});
		g.tabular([&] {
			g.node("tictactoe", [&] {
				g.append_quoted("X 0 X\n");
				g.append_quoted("X . .\n");
				g.append_quoted("0 X 0");
			});
			g.node("t_i_c_t_a_c_t_o_e", [&] {
				g.append_quoted("\nX  0  X\n\n");
				g.append_quoted(  "X  .  .\n\n");
				g.append_quoted(  "0  X  0\n");
			});
			g.node("three_lines", [&] {
				g.append_quoted("a\nb\nc");
			});
			g.node("piecewise", [&] {
				for (char c = 'a'; c <= 'z'; c++)
					g.append_quoted(String<8>(Char(c))); });
			g.node("empty",        [&] { g.append_quoted(""); });
			g.node("newline",      [&] { g.append_quoted("\n"); });
			g.node("two_newlines", [&] { g.append_quoted("\n\n"); });
		});
	});

	print_generated("quoted_bash_script", [&] (Hid_generator &g) {
		char const * const script =
			"export VERSION=`cat /VERSION`\n"
			"cp -r /rw/config/$VERSION/*  /config/\n"
			"mkdir -p /rw/depot\n"
			"cp -r /config/depot/* /rw/depot\n"
			"exit\n";
		g.append_quoted(script);
	});

	with_generated("bad_pipe_as_attribute_value",
		[&] (Hid_generator &g) { g.attribute("pipe", "|"); },
		[&] (Node const &node) {
			log(node);
			if (node.has_attribute("pipe"))
				fail("generated attribute with pipe as value");
		});

	auto bad_tag_name = [] (char c) { return String<64>("bad_", c); };

	with_generated("bad_attribute_values",

		[&] (Hid_generator &g) {
			for (char i = 0; i < 32; i++)
				g.attribute(bad_tag_name(i).string(), &i, 1);
			g.attribute("innocent", 123);
		},
		[&] (Node const &node) {
			log("node: ", node);
			if (!node.has_attribute("innocent"))
				fail("bad attribute values resulted in invalid node");
			for (char i = 0; i < 32; i++)
				if (node.has_attribute(bad_tag_name(i).string()))
					fail("generated attribute with bad value");
		});

	with_generated("name_with_colon",

		[&] (Hid_generator &g) {
			g.node("dev1", [&] { g.attribute("name", "a:b"); });
			g.node("dev2", [&] { g.attribute("name", "a: b"); });
			g.node("dev3", [&] { g.attribute("name", ":"); });
		},
		[&] (Node const &node) {
			using Value = String<10>;
			log("node: ", node);
			auto dev_name = [&] (char const *type)
			{
				return node.with_sub_node(type,
					[&] (Node const &node) {
						return node.attribute_value("name", Value()); },
					[&] {
						return Value(); });
			};
			if (dev_name("dev1") != "a:b")  fail("unexpected name of dev1");
			if (dev_name("dev2") != "a: b") fail("unexpected name of dev2");
			if (dev_name("dev3") != ":")    fail("unexpected name of dev3");
		});

	log("--- End of HID-parser test ---");
	env.parent().exit(0);
}
