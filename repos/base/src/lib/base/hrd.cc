/*
 * \brief  Parser and generator for Human-Readable Data (HRD)
 * \author Norman Feske
 * \date   2025-08-06
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/hrd.h>

using namespace Genode;


void Hrd_node::_for_each_sub_node(Span const &bytes, With_indent_span::Ft const &fn)
{
	struct Node
	{
		char const *start, *last;

		Indent indent;
		bool   enabled;

		size_t num_bytes()        const { return last - start + 1; };
		bool   contains(Indent i) const { return i.value > indent.value; }

	} node { nullptr, nullptr, { ~0U }, false };

	auto finish = [&]
	{
		if (node.start && node.last && node.enabled)
			fn(node.indent, Span(node.start, node.num_bytes()));
	};

	auto start = [&] (Indent indent, bool enabled, Span const &seg)
	{
		finish();
		node = { .start   = seg.start,
		         .last    = seg.start + seg.num_bytes - 1,
		         .indent  = indent,
		         .enabled = enabled };
	};

	auto extend = [&] (Span const &seg)
	{
		node.last = seg.start + seg.num_bytes - 1;
	};

	_for_each_segment(bytes,
		[&] (Prefix const  prefix, Indent const indent, Span const &seg) {
			if (prefix.node_or_xnode() && !node.contains(indent))
				start(indent, prefix.node(), seg);
			else
				extend(seg); });
	finish();
}


void Genode::Hrd_node::_for_each_attr(Span const &bytes, auto const &fn)
{
	auto with_tag_value = [] (Span const &s, auto const &fn)
	{
		_with_ident(s, [&] (Span const &tag, Span const &remain) {
			if (tag.num_bytes && remain.num_bytes && remain.start[0] == ':')
				remain.cut(' ', [&] (Span const &, Span const &value) {
					_with_trimmed(value, [&] (Span const &trimmed_value) {
						fn(tag, trimmed_value); }); }); });
	};

	auto tag_exists = [] (Span const &seg)
	{
			bool result = false;
			_with_ident(seg, [&] (Span const &tag, Span const &remain) {
				result = tag.num_bytes && remain.start[0] == ':'; });
			return result;
	};

	bool done = false;
	_for_each_segment(bytes,
		[&] (Prefix const prefix, Indent const, Span const &seg) {
			if (done)
				return;

			if (prefix.type == Prefix::TOP)
				seg.cut(' ', [&] (Span const &, Span const &seg) {
					_with_trimmed(seg, [&] (Span const &seg) {
						if (tag_exists(seg))
							with_tag_value(seg, fn);
						else if (seg.num_bytes)
							fn(Span("name", 4), seg);
					});
				});
			else if (prefix.type == Prefix::OTHER)
				_with_trimmed(seg, [&] (Span const &seg) {
					with_tag_value(seg, fn); });
			else
				done = true;
	});
}


void Genode::Hrd_node::_for_each_attribute(With_attribute::Ft const &fn) const
{
	_for_each_attr(_bytes, [&] (Span const &tag, Span const &value) {
		fn(Attribute { .tag   = { tag  .start, tag  .num_bytes },
		               .value = { value.start, value.num_bytes } }); });
}


void Hrd_node::_with_tag_value(char const *type, With_tag_value::Ft const &fn) const
{
	size_t const type_len = strlen(type);

	bool found = false;
	_for_each_attr(_bytes, [&] (Span const &tag, Span const &value) {
		if (!found && tag.equals({ type, type_len })) {
			fn(tag, value);
			found = true; } });
}


/**
 * Validate presence of node type and end marker for top-level node
 */
Const_byte_range_ptr Hrd_node::_validated(Const_byte_range_ptr const &bytes)
{
	bool valid = false;
	_with_type(bytes, [&] (Span const &t) { valid = (t.num_bytes > 0); });
	if (!valid)
		return { nullptr, 0 };

	/*
	 * Scan for end marker, reject node in the presence of control
	 * characters, except:
	 *
	 * - Tabs may appear within raw segments or comments
	 * - CR is followed by LF as part of a line ending
	 */
	char const control_mask = ~0x1f;
	char next = bytes.start[0];
	enum { START, ACCEPT, REJECT } tabs { };
	for (unsigned n = 1; n < bytes.num_bytes; n++) {
		char const curr = next;
		next = bytes.start[n];

		if (tabs == START && (curr == ':' || curr == '.')) tabs = ACCEPT;
		if (tabs == START && (curr != ' '))                tabs = REJECT;
		if (curr == '|' || curr == '\n')                   tabs = START;

		if (!(curr & control_mask)) {
			if (curr == '\n' && _minus(next)) /* end marker */
				return { bytes.start, n + 1 };

			if (curr == '\n')                   continue;
			if (curr == '\r' && next == '\n')   continue;
			if (curr == '\t' && tabs == ACCEPT) continue;
			break;
		}
	}
	return { nullptr, 0 };
}


Hrd_node::Hrd_node(Const_byte_range_ptr const &bytes) : _bytes(_validated(bytes)) { }


void Hrd_generator::_attribute(char const *tag, char const *value, size_t val_len)
{
	auto insert = [&] (size_t gap, auto const &fn)
	{
		_out_buffer.with_inserted_gap(_node_state.attr_offset, gap, [&] (Out_buffer &out) {
			fn(out);
			_node_state.attr_offset += gap; });
	};

	if (!_node_state.has_attr) {
		if (strcmp(tag, "name") == 0)
			insert(1 + val_len, [&] (Out_buffer &out) {
				print(out, " ", Cstring(value, val_len)); });
		else
			insert(2 + strlen(tag) + 2 + val_len, [&] (Out_buffer &out) {
				print(out, "  ", tag, ": ", Cstring(value, val_len)); });
	} else {
		insert(3 + strlen(tag) + 2 + val_len, [&] (Out_buffer &out) {
			print(out, " | ", tag, ": ", Cstring(value, val_len)); });
	}
	_node_state.has_attr = true;
}


#include <util/xml_node.h>

void Hrd_generator::_node(char const *name, Node_fn::Ft const &fn)
{
	_quoted = false;
	if (_node_state.indent.level == 0)
		print(_out_buffer, name);
	else
		print(_out_buffer, "\n", _node_state.indent, "+ ", name);

	if (_out_buffer.exceeded())
		return;

	{
		struct Orig { size_t used; Node_state node_state; };
		Orig const orig { _out_buffer.used(), _node_state, };

		_node_state = { .indent      = { _node_state.indent.level + 1 },
		                .attr_offset = _out_buffer.used(),
		                .has_attr    = false };

		struct Guard
		{
			Hrd_generator &g; Orig orig; bool ok;

			~Guard()
			{
				g._node_state = orig.node_state;
				g._quoted = false;
				if (!ok)
					g._out_buffer.rewind(orig.used);
			}
		} guard { .g = *this, .orig = orig, .ok = false };

		fn();
		guard.ok = true;
	}

	if (_node_state.indent.level == 0)
		print(_out_buffer, "\n-\n");
}


void Hrd_generator::_copy(Hrd_node const &node)
{
	auto with_stripped_indentation = [&] (Span const &line, auto const &fn)
	{
		size_t const skip = node._indent.value;
		if (line.num_bytes >= skip)
			fn(Span { line.start + skip, line.num_bytes - skip });
		else
			fn(Span { nullptr, 0 });
	};

	bool first = true;
	node._bytes.split('\n', [&] (Span const &line) {
		if (line.starts_with({ "-", 1 })) /* exclude end marker of top-level node */
			return;

		print(_out_buffer, "\n", _node_state.indent);
		if (first)
			print(_out_buffer, "+ ", Cstring(line.start, line.num_bytes));
		else
			with_stripped_indentation(line, [&] (Span const &line) {
				print(_out_buffer, "  ", Cstring(line.start, line.num_bytes)); });

		first = false;
	});
}


void Hrd_generator::node_attributes(Xml_node const &node)
{
	node.for_each_attribute([&] (auto const &attr) {
		attr.with_raw_value([&] (char const *start, size_t num_bytes) {
			attribute(attr.name().string(), start, num_bytes); }); });
}
