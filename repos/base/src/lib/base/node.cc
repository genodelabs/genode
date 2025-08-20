/*
 * \brief  Node API implementation
 * \author Norman Feske
 * \date   2025-08-20
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/node.h>

using namespace Genode;


void Node::_with_optional_sub_node(char const *type, With_node::Ft const &fn) const
{
	_process_if_valid([&] (auto const &node) {
		node.with_sub_node(type,
			[&] (auto const &sub_node) { fn(Node(sub_node)); },
			[&]                        { }); });
}


void Node::_for_each_sub_node(char const *type, With_node::Ft const &fn) const
{
	_process_if_valid([&] (auto const &node) {
		node.for_each_sub_node([&] (auto const &sub_node) {
			if (sub_node.type() == type)
				fn(Node(sub_node)); }); });
}


void Node::_for_each_sub_node(With_node::Ft const &fn) const
{
	_process_if_valid([&] (auto const &node) {
		node.for_each_sub_node([&] (auto const &sub_node) {
			fn(Node(sub_node)); }); });
}


unsigned Node::num_sub_nodes() const
{
	unsigned count = 0;
	for_each_sub_node([&] (auto const &) { count++; });
	return count;
}


void Node::_for_each_attribute(With_attribute::Ft const &fn) const
{
	_with(
		[&] (Xml const &n) {
			n.for_each_attribute([&] (Xml_attribute const &a) {
				a.with_raw_value([&] (char const *start, size_t len) {
					fn(Attribute { a.name(), { start, len } }); }); }); },
		[&] (Hrd const &n) {
			n.for_each_attribute([&] (Hrd_node::Attribute const &a) {
				fn(Attribute { .name  = { Cstring(a.tag.start, a.tag.num_bytes) },
				               .value = { a.value.start, a.value.num_bytes } }); }); },
		[&] { });
}


bool Node::has_type(char const *type) const
{
	return _process([&] { return Type { "empty" } == type; },
	                [&] (auto const &node) { return node.has_type(type); });
}


bool Node::has_sub_node(char const *type) const
{
	bool result = false;
	with_optional_sub_node(type, [&] (Node const &) { result = true; });
	return result;
}


Node::Type Node::type() const
{
	return _process([] { return Type { "empty" }; }, [&] (auto const &node) {
		return node.type(); });
}


bool Node::has_attribute(char const *attr) const
{
	return _process([] { return false; }, [&] (auto const &node) {
		return node.has_attribute(attr); });
}


size_t Node::num_bytes() const
{
	return _with([&] (Xml const &n) { return n.size(); },
	             [&] (Hrd const &n) { return n.num_bytes(); },
	             [&] () -> size_t   { return 0; });
}


bool Node::differs_from(Node const &other) const
{
	return _with(
		[&] (Xml const &n) {
			return other._with(
				[&] (Xml const &other_n) { return n.differs_from(other_n); },
				[&] (Hrd const &)        { return true; },
				[]                       { return true; }); },
		[&] (Hrd const &n) {
			return other._with(
				[&] (Xml const &)        { return true; },
				[&] (Hrd const &other_n) { return n.differs_from(other_n); },
				[]                       { return true; }); },
		[&] {
			return other._with(
				[&] (Xml const &) { return true; },
				[&] (Hrd const &) { return true; },
				[]                { return false; });
	});
}


void Node::print(Output &out) const
{
	_process_if_valid([&] (auto const &node) { node.print(out); });
}


void Node::Quoted_content::print(Output &out) const
{
	_node.for_each_quoted_line([&] (auto const &line) {
		line.print(out);
		if (!line.last) out.out_char('\n'); });
}


void Node::_for_each_quoted_line(With_quoted_line::Ft const &fn) const
{
	_process_if_valid([&] (auto const &node) {
		node.for_each_quoted_line([&] (auto const &l) {
			fn(Quoted_line { *this, l.bytes.start, l.bytes.num_bytes,
			                        l.last }); }); });
}


Node::Node(Const_byte_range_ptr const &bytes)
{
	_with_skipped_whitespace(bytes, [&] (Const_byte_range_ptr const &bytes) {
		if (bytes.start[0] == '<') {
			try { _xml.construct(bytes); } catch (...) { }
		} else {
			_hrd.construct(bytes);
			if (!_hrd->valid()) _hrd.destruct();
		}
	});
}


Node::Node(Node const &other, Byte_range_ptr const &dst)
{
	other._with(
		[&] (Xml const &node) {
			node.with_raw_node([&] (char const *start, size_t num_bytes) {
				if (dst.num_bytes >= num_bytes) {
					memcpy(dst.start, start, num_bytes);
					_xml.construct(dst.start, num_bytes); } });
		},
		[&] (Hrd const &node) { _hrd.construct(node, dst); },
		[&]                   { });
}


void Generator::_node(char const *name, Callable<void>::Ft const &fn)
{
	if (_xml_ptr) _xml_ptr->node(name, fn);
	if (_hrd_ptr) _hrd_ptr->node(name, fn);
}


void Generator::attribute(char const *name, char const *value, size_t n)
{
	if (_xml_ptr) _xml_ptr->attribute(name, value, n);
	if (_hrd_ptr) _hrd_ptr->attribute(name, value, n);
}


void Generator::attribute(char const *name, char const *str)
{
	attribute(name, str, strlen(str));
}


void Generator::attribute(char const *name, bool value)
{
	attribute(name, value ? "true" : "false");
}


void Generator::attribute(char const *name, long long value)
{
	attribute(name, String<64>(value));
}


void Generator::attribute(char const *name, unsigned long long value)
{
	attribute(name, String<64>(value));
}


void Generator::attribute(char const *name, double value)
{
	attribute(name, String<64>(value));
}


void Generator::node_attributes(Node const &node)
{
	if (_xml_ptr) _xml_ptr->node_attributes(node);
	if (_hrd_ptr) _hrd_ptr->node_attributes(node);
}
